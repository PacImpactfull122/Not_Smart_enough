# Roadmap

Fases com criterio de verificacao objetivo. Cada fase termina com comando executavel e evidencia; sem evidencia a fase nao e concluida.

## Fase 0 - Fundacao (atual)

- [x] clonar upstream para referencia em /tmp/opencode-ref
- [x] documentar visao, tecnologias, contexto e arquitetura
- [ ] validar escolhas de dependencias com build minimo (CMake + sqlite3 + tree-sitter + curl)
- [ ] definir schema minimo de `session`, `message`, `part`, `usage`

Verificacao: `cmake -S . -B build && cmake --build build` executa e linked as quatro libs.

## Fase 1 - Loop basico

- CLI com `run <prompt>`
- SessionStore em SQLite (create, append message, list)
- um provider OpenAI-compatible com stream
- system prompt + tools minimos (`read`, `write`, `edit`, `shell`)
- arena por request

Verificacao: rodar um prompt real em um repo de teste e conferir saida streamada, mensagens persistidas e RSS estavel apos 10 requests.

## Fase 2 - Contexto completo

- select/load separados
- instrucoes de AGENTS.md/CLAUDE.md com ordem cacheavel
- historico com boundary e compaction local (auto + manual)
- tool snapshot por permissao
- agentes `build` e `plan`

Verificacao: teste de regressao que reproduce um prompt com 200 mensagens e confirma que a compactacao reduz tokens e mantem `keep` verbatim.

## Fase 3 - Indexacao e poda

- tree-sitter incremental com cache em SQLite
- busca por simbolo e por texto
- poda de AST com flag, desligada por padrao
- metricas de token removido por arquivo

Verificacao: corpus de 20 tarefas reais com poda ligada/desligada; comparar tokens e acerto. So ativar poda se nao houver regressao de acerto.

## Fase 4 - Multi-agente e shared memory

- scheduler MPMC com cancellation
- subagentes com depth e inbox
- EventBus com ring buffer
- daemon + HTTP/WS
- shared memory do indice com double-buffer

Verificacao: TSan limpo em teste com 8 agentes concorrentes; latencia de leitura do indice via shm menor que via SQLite.

## Fase 5 - Paridade e polishing

- providers adicionais (Anthropic, Google)
- MCP basico
- TUI
- plugins (definir formato; JS via embedding e custo a avaliar)

Verificacao: executar um subset dos testes de comportamento do upstream e comparar resultados.

## Decisoes em aberto

1. Coroutines ou thread pool para stream? Começar com thread pool, medir, decidir depois.
2. cpp-httplib ou Boost.Beast? httplib para o seed, Beast se WS/portabilidade exigirem.
3. Gerador de schema? Sem gerador ate os structs estabilizarem.
4. Embedding de JS para plugins? Fora do escopo ate Fase 5; se entrar, avaliar QuickJS vs. nao entrar.
