# Roadmap

Fases com criterio de verificacao objetivo. Cada fase termina com comando executavel e evidencia; sem evidencia a fase nao e concluida.

## Fase 0 - Fundacao (atual)

- [x] clonar upstream para referencia em /tmp/opencode-ref
- [x] documentar visao, tecnologias, contexto, arquitetura e sandbox
- [ ] validar escolhas de dependencias com build minimo (CMake + libpq + hiredis + tree-sitter + libcurl)
- [ ] subir Postgres e Redis locais e definir schema minimo
- [ ] tabela `session`, `message`, `part`, `usage`, `workspace`

Verificacao: `cmake -S . -B build && cmake --build build` executa e linked as libs; `psql` e `redis-cli` conectam.

## Fase 1 - Loop basico

- CLI com `run <prompt>`
- SessionStore em Postgres, cache em Redis
- um provider OpenAI-compatible com stream
- system prompt + tools minimos (`read`, `write`, `edit`, `shell`)
- arena por request

Verificacao: rodar um prompt real em um repo de teste e conferir saida streamada, mensagens persistidas e RSS estavel apos 10 requests.

## Fase 2 - Sandbox e contexto compacto

- `sandbox.run`, `sandbox.calc`, `sandbox.test` com isolamento completo
- `workspace.write` / `workspace.read` no Postgres
- DSL compacta com encode/decode e medicao de reducao de tokens
- meta: `tokens(dsl) / tokens(prosa) < 0.70` em 20 prompts de teste
- comparar acerto com e sem DSL; reduzir tokens so se o acerto nao cair

Verificacao: testes de seccomp, rlimit, isolamento de filesystem e integridade de referencias da DSL.

## Fase 3 - Contexto completo

- select/load separados
- instrucoes de AGENTS.md/CLAUDE.md com ordem cacheavel
- historico com boundary e compactacao local (auto + manual)
- tool snapshot por permissao
- agentes `build` e `plan`

Verificacao: teste de regressao com 200 mensagens confirmando que a compactacao reduz tokens e mantem `keep` verbatim.

## Fase 4 - Indexacao e poda

- tree-sitter incremental com cache no Postgres
- busca por simbolo e por texto
- poda de AST com flag, desligada por padrao
- metricas de token removido por arquivo

Verificacao: corpus de 20 tarefas reais com poda ligada/desligada; so ativar se nao houver regressao de acerto.

## Fase 5 - Grafo e ambiente 3D

- tabelas `node`/`edge` com proveniencia por `source_seq`
- ferramentas `graph.upsert/link/neighbors/search`
- `sim.run`, `sim.compare`, `sim.probe` com Bullet em processo isolado
- UI HTML/CSS para series e estado final, sem JavaScript

Verificacao: queda livre contra solucao analitica; traversia limitada; nenhuma ferramenta de escrita disponivel para `plan`.

## Fase 6 - Multi-agente, daemon e UI

- scheduler MPMC com cancellation
- subagentes com depth e inbox
- EventBus com ring buffer
- daemon + HTTP/SSE servindo HTML/CSS puro
- shared memory do indice com double-buffer

Verificacao: TSan limpo com 8 agentes concorrentes; UI sem nenhuma linha de JavaScript; SSE entregando deltas.

## Fase 6 - Paridade

- providers adicionais (Anthropic, Google)
- MCP basico
- TUI opcional em C/C++
- comparacao com subset de comportamentos do upstream

## Decisoes em aberto

1. Coroutines ou thread pool para stream? Comecar com thread pool, medir, decidir depois.
2. cpp-httplib ou Boost.Beast? httplib para o seed; Beast se SSE/WS exigirem.
3. Gerador de schema? Sem gerador ate os structs estabilizarem.
4. Formato da DSL pode precisar de ajuste depois da primeira medicao de tokens; trata-la como interna ate Fase 3.
