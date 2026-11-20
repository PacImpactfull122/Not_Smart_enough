# Plano mestre: IA, sandbox e leveza

Este documento amarra todos os subsistemas em um unico fluxo de execucao e define como medir "mais inteligente" e "leve" sem transformar isso em slogan.

## 1. Como a IA funciona, ciclo a ciclo

Cada interacao e um `Turn`. Um turn tem seis etapas fixas. Nenhuma etapa pode pular a anterior.

```
[1] PERCEBER --> [2] RECUPERAR --> [3] COMPILAR --> [4] DECIDIR --> [5] AGIR --> [6] OBSERVAR
                                                                                    |
                                                                                    +--> volta a [1]
```

### [1] Perceber

Entradas:

- prompt do usuario (nao-confiavel);
- arquivos do repo indexados por tree-sitter;
- instrucoes do projeto (AGENTS.md, CLAUDE.md);
- estado da sessao (Postgres) e cache (Redis);
- nos do grafo relevantes para o arquivo atual.

Regra: tudo que vem de fora e marcado com origem. Nada entra no prompt sem `source_seq` ou `source_path`.

### [2] Recuperar

Seleciona o que entra no contexto, nesta ordem de prioridade:

1. arquivo aberto e trechos citados no prompt (sempre inclusos, nunca podados);
2. simbolos chamados diretamente pelo arquivo aberto;
3. decisoes e testes do grafo ligados a esses simbolos;
4. resultados de sandbox recentes (`workspace`);
5. historico recente (`keep` da compactacao);
6. instrucoes e catalogo de tools.

O limite e um orcamento de tokens por turn, nao um numero fixo de caracteres.

### [3] Compilar

Monta o contexto em duas camadas:

- **camada estavel**: catalogo de tools, instrucoes de usuario, esquema do grafo, regras de formato. Vira prefixo cacheavel.
- **camada variavel**: prompt, arquivos, simbolos, resultados de sandbox, historico.

A camada estavel e serializada pela DSL compacta (`docs/02-contexto.md`, secao 7). A camada variavel entra em texto, porque exige exatidao (codigo, diffs, stack traces).

Se um modelo local com injecao de embeddings estiver configurado, o compressor vetorial (`docs/06-compressao-vetorial.md`) pode substituir a DSL na camada estavel. Sem modelo, a DSL e o unico caminho.

### [4] Decidir

O modelo escolhe entre:

- responder em texto;
- chamar uma ferramenta de edicao (`read`, `write`, `edit`, `shell`);
- chamar o sandbox (`sandbox.run`, `sandbox.calc`, `sim.run`);
- consultar o grafo (`graph.neighbors`, `graph.search`);
- pedir compactacao (auto, overflow ou manual).

A escolha e restrita pelo snapshot de tools calculado no `select`. Agente `plan` nao recebe ferramentas de escrita nem `sandbox.run`.

### [5] Agir

Toda acao passa pelo mesmo contrato:

```
ToolCall { id, name, args, permission_check, timeout, budget }
```

- argumentos validados por allowlist no servidor;
- permissao conferida por objeto + acao, nunca so por nome de ferramenta;
- execucao em processo separado quando envolve codigo ou simulacao;
- saida truncada com cabeçalho explicando o que foi omitido;
- **limite duro de tool calls por turn** e detector de loop de checagem.

O limite existe por causa do `OverServe` (arXiv `2611.14808`): cada tool call termina normalmente, mas o agente repete a checagem de uma condicao inacabada e amplifica requisicoes de API em ate **93.88x**, passando nos checks originais. Timeout por execucao so nao resolve — e preciso contar chamadas no nivel do turn.

### [6] Observar

O resultado da acao volta como tool result, nunca como instrucao. O agente registra no grafo:

- no `test_result` ou `sandbox_result` com `source_seq`;
- edge `derives_from` ligando ao no que originou a acao;
- edge `refutes` se o resultado contradiz uma hipotese anterior.

Depois o ciclo recomeça com o novo estado.

## 2. Como o sandbox funciona

Detalhe em `docs/05-sandbox.md` e `docs/08-ambiente-3d.md`. Resumo operacional:

### 2.1 Tipos de ambiente

| Ambiente | O que executa | Isolamento |
| --- | --- | --- |
| `code` | C/C++ compilado, testes, expressoes matematicas | processo + rlimit + seccomp + sem rede |
| `sim` | cenas Bullet/Jolt, probes, comparacao | mesmo de `code` + allowlist de campos |
| `workspace` | memoria de trabalho da sessao | Postgres, sem execucao |

### 2.2 Sequencia de uma execucao

```
agente --(spec JSON)--> validacao --> spawn isolado --> execucao com timeout
                                                          |
                                                    stdout/stderr/metricas
                                                          |
                                                    sanitizacao --> tool result
                                                          |
                                                    grafo + workspace
```

Validacao acontece **antes** do spawn. Campo desconhecido na spec e recusado sem iniciar processo.

### 2.3 O que o sandbox garante

- nenhum acesso a rede;
- nenhuma credencial do provider;
- escrita so em `/sandbox/out`;
- CPU, memoria, tamanho de arquivo e numero de processos limitados;
- timeout duro com kill do grupo;
- saida sem paths absolutos do host e sem variaveis de ambiente.

### 2.4 O que o sandbox nao faz

- nao decide o que e correto — apenas produz observacao;
- nao substitui teste de regressao no repo;
- nao roda Python, JS nem interpretador de linguagem externa;
- nao persiste estado dentro do processo isolado.

## 3. Como tornar as IAs muito mais inteligentes

"Mais inteligente" aqui nao e modelo maior. E melhor uso do mesmo modelo, com custo controlado. Sete mecanismos, em ordem de custo-beneficio.

### 3.1 Memoria com proveniencia (grafo)

O agente nao recomeca do zero a cada turn. Decisoes, hipoteses, testes e refutacoes ficam no grafo com `source_seq`.

Ganho: menos repeticao de raciocinio, contexto mais denso. `Mind the Gap` (arXiv `2611.23142`) mede 71.4% de preferencia por respostas apos reparo do conhecimento.
Custo: uma tabela Postgres e duas consultas por turn.
Risco: no falso influencia respostas futuras. **Atenção**: `Forgetting Without Restarting` (arXiv `2609.04875`) prova que apagar o registro plaintext **nao** remove resumos nem KV cache derivados — instruction-based forgetting tem Leak@probes = 1.00. Por isso correcao exige `graph.forget` com replay seletivo a partir de `source_seq`, nao so edge `refutes`.

### 3.2 Verificacao por execucao (sandbox)

Em vez de "acreditar" que um codigo funciona, o agente executa. Matematica 3D e fisica permitem testar hipoteses contra simulacao.

Ganho: erros detectados antes de virar codigo no repo.
Custo: processo isolado por execucao.
Risco: resultado da simulacao pode ser interpretado mal — mitigado por probes nomeados e solucao analitica de referencia.

### 3.3 Multi-agente com papéis separados

| Agente | Papel | Permissao |
| --- | --- | --- |
| `build` | implementa | escrita + execute |
| `plan` | analisa | somente leitura + calc |
| `general` | busca profunda | leitura + grafo |
| `review` | revisa diff | leitura + sandbox de teste |
| `safety` | bloqueia acoes inseguras | leitura + autoridade de veto |
| `compact` | resume contexto | sem ferramentas de repo |

O agente `safety` nao e opcional. Paper `Aligning AI Teams` (arXiv `2611.17820`) mostra que critica mutua e group chat **nao** restauram o alignment de um unico agente: o que funciona e um **safety lead designado** com autoridade sobre o sistema. Sem esse papel, a difusao de responsabilidade faz o time ignorar falhas que um agente sozinho detectaria.

Ganho: um agente nao precisa carregar o mesmo contexto que outro; revisao e independente de quem escreveu. `WorkflowArena` (arXiv `2611.12856`) mostra que a organizacao do workflow e fator mensuravel de desempenho, e que workflows melhores melhoram ate executores mais fracos.
Custo: `katIA-MA` (arXiv `2611.10958`) mede 0.570s -> 11.214s e 1.235 -> 6.462 tokens ao passar de prompt unico para 3 agentes. Multi-agente e opt-in por tarefa.
Risco: agentes discordando e difusao de responsabilidade. Resolvido por: hipoteses independentes antes de votar (`2611.23294`), edge `refutes` no grafo, e o agente `safety` com veto (`2611.17820`).

### 3.4 Contexto por relevancia, nao por proximidade

Ordenar por: simbolo citado > simbolo chamado > mesmo modulo > todo o repositorio. Isso e melhor que "ultimas N mensagens" porque traz o que importa, nao o que e recente.

Ganho: menos tokens para a mesma informacao util.
Custo: indexacao tree-sitter + consulta ao grafo.
Risco: poda remover linha relevante — arquivo aberto nunca e podado.

### 3.5 Prompt caching agressivo

Prefixo estavel, ordem determinstica, breakpoint explicito no ultimo bloco estavel, hash no Redis.

Ganho: latencia menor e custo menor por token de prefixo. Anthropic documenta read a 0.1x do preco de input (0.05x em alguns modelos).
Custo: disciplina de ordenacao e de posicionamento do breakpoint.
Risco: breakpoint em bloco variavel (timestamp) gera miss permanente. Campos que variam por request ficam **depois** do breakpoint. Lookback e de 20 blocos; se conversa crescer mais, adicionar segundo breakpoint.

### 3.6 Compressao em duas trilhas

DSL agora, vetorial quando possivel. Nunca substituir codigo exato por vetor.

Ganho: retencao ajustada por classe; ate 50% de reducao em prosa de instrucoes.
Custo: implementacao da DSL e medicao por classe.
Risco: `CompressAgent` (arXiv `2608.01056`) mede queda de 93.8% para 19.9-47.0% de sucesso quando a retencao cai a 35%. Definicoes de tools e specs de matematica **nunca** sao comprimidas; `Referential Dangling` (`2608.04569`) exige manter pares simbolo-definicao juntos.

### 3.7 Aprendizado com feedback

Toda correcao do usuario cria edge `refutes` e aumenta a confianca da correcao. Na proxima sessao, a mesma situacao carrega a decisao corrigida.

Ganho: erro repetido diminui.
Custo: registro de feedback no grafo.
Risco: memorizar preferencia errada — versionar por `key` e permitir desfazer.

### Ordem de prioridade para implementar

1. memoria com proveniencia (3.1)
2. verificacao por execucao (3.2)
3. contexto por relevancia (3.4)
4. prompt caching (3.5)
5. multi-agente (3.3)
6. compressao (3.6)
7. feedback (3.7)

Motivo: os primeiros quatro melhoram o resultado com o mesmo modelo e sem treino. Os dois ultimos exigem mais infraestrutura.

## 4. Como manter o OpenCode leve

Leveza e orcamento, nao sensacao. Numeros abaixo sao metas de aceite, nao resultados medidos.

### 4.1 Orcamento de recursos

| Recurso | Meta | Como medir |
| --- | --- | --- |
| binario release (strip) | < 15 MB | `size` e `du -h` |
| startup ate pronto | < 50 ms | `hyperfine` |
| RSS em repouso | < 40 MB | `/proc/<pid>/status` |
| RSS por request (10 sequencias) | crescimento < 5% | `smaps_rollup` |
| alocacoes por request | arena unica, zero free isolado | contador no PMR |
| dependencias de runtime | so libc/libstdc++/libpq/hiredis/curl | `ldd` |
| threads em repouso | 4 | `ps -T` |

### 4.2 Regras de leveza

1. **Nada de runtime pesado**: sem Node, sem Bun, sem Python, sem JVM, sem JS engine.
2. **Linkagem dinamica controlada**: cada lib nova precisa justificar o custo de `ldd`.
3. **Arena por request**: objeto temporario nunca e `new` isolado; tudo vem do PMR.
4. **Modelo local e opcional**: llama.cpp/vLLM so linkados se o build de compressao vetorial for explicitamente habilitado.
5. **Ferramentas caras sob demanda**: Bullet so carregado quando `sim.run` e chamado; o sink de sim nao fica residente no startup.
6. **Cache em Redis, nao em memoria do processo**: estado derivado nao infla o RSS.
7. **Build por features**: `-DOPENCODE_WITH_SIM=OFF`, `-DOPENCODE_WITH_VECTOR=OFF` por padrao.

### 4.3 Conflito inteligencia vs leveza

Sempre que um mecanismo de inteligencia adiciona peso, ele vira modulo opcional com flag de build. Exemplos:

| Mecanismo | Impacto | Solucao |
| --- | --- | --- |
| Bullet | ~15 MB de lib | modulo `sim`, desligado no build leve |
| llama.cpp | dezenas de MB | modulo `vector`, desligado por padrao |
| tree-sitter gramaticas | algumas centenas de KB por lingua | compilar so as 6 iniciais |
| grafo no cliente | nenhum, fica no Postgres | cliente guarda so IDs |

Regra: o binario leve e o padrao; inteligencia extra e opt-in e medida.

## 5. Sequencia de implementacao

| Fase | Entrega | Criterio objetivo |
| --- | --- | --- |
| 0 | build + Postgres/Redis | cmake build, psql e redis-cli conectam |
| 1 | loop basico com provider | prompt real, stream, mensagens persistidas |
| 2 | sandbox + DSL | seccomp/rlimit/testes de isolamento; retencao >= 70% por classe |
| 3 | contexto completo | 200 mensagens compactam com `keep` verbatim |
| 4 | indexacao e poda | corpus de 20 tarefas sem regressao de acerto |
| 5 | grafo + 3D | queda livre bate com analitica; proveniencia validada |
| 6 | multi-agente + UI | TSan limpo; UI sem JavaScript |
| 7 | paridade | subset de testes do upstream passa |

Detalhes em `docs/04-roadmap.md`.

## 6. Metricas de aceite do projeto inteiro

Nao declarar sucesso sem estas medidas:

1. **Acerto**: tarefas de codigo resolvidas antes e depois de cada mecanismo de inteligencia. Sem regressao.
2. **Tokens**: `tokens(compilado) / tokens(prosa)`, alvo < 0.70 na camada estavel.
3. **Latencia**: p50 e p95 do turn, com e sem cache.
4. **Memoria**: RSS em repouso e crescimento por request, conforme secao 4.1.
5. **Isolamento**: testes de seccomp, rlimit, filesystem e rede do sandbox passando.
6. **Reproducibilidade**: mesmo hash de cena e mesma entrada produzem mesma saida.

## 7. Invariantes

- Toda acao do agente e observavel e registrada no grafo.
- Nenhum conteudo nao-confiavel vira instrucao.
- Nenhum mecanismo de inteligencia entra no build padrao sem medicao de custo.
- Nenhuma otimizacao de token reduz exatidao de codigo, diff ou stack trace.
- Leveza e configuracao default; recursos pesados sao opt-in.
