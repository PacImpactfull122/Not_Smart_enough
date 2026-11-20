# Modelo de contexto

Contexto e o conjunto ordenado de informacoes enviado ao modelo em cada request. O objetivo e ser deterministico: dado o mesmo estado persistido, a mesma selecao de agente e o mesmo snapshot de tools, o contexto gerado e identico.

## Visao do fluxo

```
prompt do usuario
      |
      v
[1] select(sessionID)  --> Selection { session, agent, instructions, tools }
      |
      v
[2] load(Selection)    --> Loaded { model, instructions, messages }
      |
      v
[3] build_messages     --> vector<LLMMessage> (system + historico + usuario)
      |
      v
[4] compact (se precisar) --> resumo + keep + janela nativa
      |
      v
provider (stream)
```

Esta divisao espelha `packages/core/src/session/context.ts` do upstream. A separacao existe porque `select` e reutilizado por operacoes auxiliares (titulo, compactacao, comandos) que nao precisam carregar o historico completo.

## 1. Selection

`Selection` e imutavel depois de construida.

| Campo | Origem | Regra |
| --- | --- | --- |
| `session` | Postgres por ID | Deve pertencer ao `location` atual (directory + workspaceID) |
| `agent` | registro de agentes | Resolvido por ID da sessao; cai para `build`; `mode == subagent`/`hidden` nao e selecionavel |
| `instructions` | fontes combinadas | Ordem estavel pensada para cache de prefixo |
| `tools` | snapshot do registro | Filtrado por permissoes efetivas (agente ∩ sessao) |

### Permissoes

`effective = merge(agent.permissions, session.permissions)`. O snapshot de tools e calculado a partir das permissoes efetivas, nao de uma lista global. Um agente sem permissao de `execute` nao recebe Code Mode e mantem tools diretos.

### Ordem das instrucoes (prefixo cacheavel)

Do mais compartilhado para o mais especifico:

1. catalogo Code Mode
2. instrucoes MCP
3. referencias
4. skills
5. instrucoes de usuario/projeto (AGENTS.md, CLAUDE.md e equivalentes)
6. data e ambiente (variam por dia/diretorio)

Essa ordem e intencional: o prefixo longo e estavel entre sessoes, o que permite reutilizar o prompt cache do provider. Quebrar a ordem quebra o cache.

## 2. Loaded

`load` acrescenta:

- `model` resolvido: `session.model` > `agent.model` > modelo padrao do provider, com variant.
- `messages`: historico ativo da sessao.

Modelo e historico sao carregados em paralelo; a resolucao de modelo depende dos providers disponiveis, que e verificada no `select`.

## 3. Historico e fronteiras de compactacao

Mensagens sao persistidas com `seq` monotono por sessao. Tipos relevantes: `user`, `assistant`, `tool`, `compaction`, `synthetic`.

### Boundary

A leitura do historico aceita uma fronteira:

- `latest`: usa a compactacao mais recente, inclusive janelas nativas.
- `local`: usa apenas resumos locais; ignora janelas nativas do provider.
- `model ref`: usa janela nativa somente se foi produzida pelo mesmo provider/model e ainda decodifica.

Motivo: uma janela nativa e um formato proprietario do provider. Replaya-la em outro modelo pode falhar silenciosamente. O codigo do upstream trata janela que nao decodifica como "skip", nao como erro de sessao.

### Reconstrucao

O historico ativo e:

```
[resumo da ultima compactacao aplicavel]
+ [mensagens com seq > keepID mantidas verbatim]
```

`metadata.keepID` referencia as mensagens mantidas; seu texto e reconstruido a partir do banco, nao duplicado no resumo. Isso evita divergencia entre resumo e transcricao original.

## 4. Construcao de mensagens LLM

`toLLMMessages` converte o modelo interno de mensagens para o formato do provider:

- texto: `TextPart`
- imagens/PDF: `MediaPart` com MIME e nome de arquivo
- anexos de diretorio: enumerados como texto
- tool calls e results: pares com `callID`
- reasoning: preservado apenas se o provider aceitar

Anexos com `source.type != uri` ou URI sem protocolo `file:` nao tem caminho local confiavel; o comportamento e incluir os dados inline, nunca inventar um path.

## 5. Compactacao

### Gatilhos

| Gatilho | Quando |
| --- | --- |
| `auto` | estimativa passa do limite configurado (padrao: 15% livre, minimo 16k) |
| `overflow` | provider rejeitou o contexto por tamanho |
| `manual` | comando explicito do usuario |

### Configuracao

```
auto: bool
buffer: percentual ou tokens
keep: tokens de conversa recente mantidos verbatim
background: bool
```

### resultado

- `text`: resumo
- `native.messages`: janela do provider, opcional
- `metadata.keepID`: ate onde o resumo cobre

### precompute

Quando `background` esta ativo, um resumo e calculado fora do caminho critico assim que o contexto se aproxima do limite. A compactacao automatica posterior troca o resumo sem esperar uma nova chamada ao modelo. O fiber de precompute carrega `usage` da sumarizacao para nao perder custo.

## 6. Compressao de prompt com AST (novidade do rewrite)

A poda de AST atua antes da compactacao por resumo, sobre arquivos de codigo que entram no contexto.

Regras propostas:

1. manter assinaturas (nome, parametros, tipo de retorno) sempre;
2. manter imports e exports;
3. remover corpo de funcoes nao relacionadas ao simbolo-alvo;
4. manter comentarios que carregam decisao ou risco (`!` e `*` no padrao do projeto);
5. nunca podar o arquivo que o usuario abriu explicitamente;
6. registrar por arquivo quantos tokens foram removidos, para medir o ganho real.

Peso de relevancia: simbolo mencionado no prompt > simbolo referenciado pelo arquivo atual > simbolo do mesmo modulo > demais.

### Risco

Poda agressiva pode remover justamente a linha necessaria para entender um bug. Por isso a poda e controlada por um flag por request e desligada por padrao no milestone 1. O ganho so entra em producao se medido com um corpus de tarefas reais.

## 7. Compilacao de contexto (DSL compacta)

Meta: o contexto serializado em forma nao-humana deve ocupar **menos de 70% dos tokens** da mesma informacao em prosa, mantendo a capacidade de reconstruir o original.

### Por que DSL e nao compressao binaria

Comprimir com zstd/lz4 reduz bytes, mas o modelo recebe texto: descomprimir antes do envio nao economiza tokens. Emitir IDs de token do provider exigiria API proprietaria e quebraria troca de modelo. Embeddings removem exatidao. A DSL compacta e a unica opcao que reduz tokens de verdade e continua reversivel.

### Formato

```
S:<session_id>|A:<agent_id>|M:<model_ref>
I:<id>:<hash>           # instrucoes, uma linha por fonte
T:<id>:<name>:<flags>   # tools, sem descricao longa
H:<seq>|<kind>|<ref>    # historico, ref aponta para payload externo
W:<slot_key>|<kind>|<hash>  # workspace de raciocinio
C:<keep_id>|<summary_hash>  # compactacao
```

Regras:

1. chaves de um caractere, valores separados por `|` e `:`;
2. strings longas (instrucoes, resumos, codigo) ficam em tabela externa com hash; o prompt leva so o hash e o trecho minimo necessario;
3. nomes repetidos viram IDs internos (`@0`, `@1`) definidos uma vez no cabecalho;
4. ordem determinstica: sessao, agente, instrucoes, tools, workspace, historico, compactacao;
5. numeros em base decimal curta, sem zeros desnecessarios;
6. nenhum texto de prosa explicativa ("o usuario pediu", "a ferramenta retornou") — o kind carrega essa semantica.

### Medicao

O limite de 70% e verificavel:

```
tokens(dsl) / tokens(prosa) < 0.70
```

Medir com o contador do provider antes do envio e salvar o resultado em `usage`. Se um provider nao expor contagem, usar o contador local aproximado e marcar a medicao como estimada.

### Riscos

- DSL demasiado criptica pode degradar a compreensao do modelo. Medir acerto em tarefas, nao so reducao de tokens.
- Hash sem o conteudo correspondente gera referencia quebrada. Validar antes de montar o prompt.
- A DSL e um formato interno: nao expor como parte da API publica sem estabilizar primeiro.

## 8. Invariantes

- Mensagens sao append-only; correcoes viram novas mensagens (`synthetic`), nunca update in-place.
- Nenhuma ferramenta recebe mais permissao do que o snapshot calculado no `select`.
- Nenhuma janela nativa e replayada em modelo diferente do que a produziu.
- Todo request tem um orcamento de tokens; exceder dispara compactacao, nao truncamento silencioso.
- Contexto gerado deve ser reproduzivel a partir do banco + configuracao, sem estado global mutavel.
- Toda referencia da DSL deve resolver para um payload existente antes do envio.
- Resultado de sandbox entra no contexto como tool result sanitizado, nunca como instrucao.
