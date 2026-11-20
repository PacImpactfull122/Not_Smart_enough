# Modelo de contexto

Contexto e o conjunto ordenado de informacoes enviado ao modelo em cada request. O objetivo e ser determinstico: dado o mesmo estado persistido, a mesma selecao de agente e o mesmo snapshot de tools, o contexto gerado e identico.

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
| `session` | SQLite por ID | Deve pertencer ao `location` atual (directory + workspaceID) |
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

Anexos com `source.type != file:` nao tem caminho local confiavel; o comportamento e incluir os dados inline, nunca inventar um path.

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

Quando `background` esta ativo, um resumo e calculado fora do caminho critico assim que o contexto se aproxima do limite. ACompactacao automatica posterior troca o resumo sem esperar uma nova chamada ao modelo. O fiber de precompute carrega `usage` da sumarizacao para nao perder custo.

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

## 7. Invariantes

- Mensagens sao append-only; correcoes viram novas mensagens (`synthetic`), nunca update in-place.
- Nenhuma ferramenta recebe mais permissao do que o snapshot calculado no `select`.
- Nenhuma janela nativa e replayada em modelo diferente do que a produziu.
- Todo request tem um orcamento de tokens; exceder dispara compactacao, nao truncamento silencioso.
- Contexto gerado deve ser reproduzivel a partir do banco + configuracao, sem estado global mutavel.
