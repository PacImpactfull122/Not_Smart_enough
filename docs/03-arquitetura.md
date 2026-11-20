# Arquitetura

## Processos e threads

```
opencode-cpp (CLI)
   |-- main thread: TUI / argumentos
   |-- scheduler thread: fila de jobs de sessao
   |-- worker threads (N = hardware_concurrency): execucao de agentes/tools
   |-- io thread: HTTP/WS stream, event loop por plataforma
   `-- indexer thread: tree-sitter + SQLite writes
```

O daemon (`opencode-cpp serve`) roda como processo separado e expoe HTTP/WS. A CLI pode operar local (sem daemon) ou conectada. A comunicacao CLI <-> daemon usa o mesmo protocolo de RPC do servico de sessao.

## Modulos

| Modulo | Responsabilidade | Dependencies |
| --- | --- | --- |
| `core::session` | sessoes, mensagens, seq, fork, revert | sqlite, schema |
| `core::context` | select/load/build/compact | session, agent, tool, instruction |
| `core::agent` | registro, selecao, permissoes, definicoes | schema |
| `core::tool` | registro, snapshot por permissao, execucao | permission, filesystem |
| `core::model` | providers, resolucao, retry, fallback | http |
| `core::index` | tree-sitter, symbol table, busca | tree-sitter, postgres |
| `core::memory` | arenas, pools, shared memory | - |
| `core::scheduler` | fila MPMC, cancellation, job lifecycle | - |
| `core::context_codec` | DSL compacta, encode/decode, medicao de tokens | context, model |
| `core::sandbox` | isolamento, execucao, workspace | scheduler, postgres |
| `app::web` | HTML/CSS server-side, SSE | core, protocol |
| `app::cli` | parse de argumentos, TUI | core |
| `app::server` | HTTP/WS, autenticacao local | core |
| `app::protocol` | tipos de RPC compartilhados | schema |

Regra de dependencia: `core` nunca depende de `app`. `app` compoe `core`.

## Modelo de estado

Fonte de verdade: PostgreSQL (tabelas `session`, `message`, `part`, `usage`, `workspace`, `index_file`, `index_symbol`). Redis guarda cache de contexto, fila de jobs, locks e rate limit; pode ser perdido e reconstruido.

Estado em memoria e derivado e pode ser reconstruido:

- `SessionRegistry`: mapa `SessionID -> SessionState` com mutex por sessao.
- `AgentRegistry`: imutavel depois do load de configuracao.
- `ToolRegistry`: snapshot gerado por request.
- `IndexCache`: tabela de simbolos com ChunkID para incrementar tree-sitter.
- `ContextCache`: versao compacta do contexto em Redis, chaveada por hash de sessao+seq+config.

### Concorrencia

- Cada sessao tem um mutex proprio. Duas sessoes nunca bloqueiam uma a outra.
- Jobs de uma mesma sessao sao serializados (evita PromptConflict/InboxConflict do upstream).
- Agents differentes em sessoes differentes rodam em paralelo.
- Escritas no Postgres passam por pool de conexoes com transacoes curtas; locks de sessao ficam no Redis com TTL.
- Jobs de sandbox rodam em processo separado, fora do pool de threads do daemon.

## Multi-agente

Um `AgentRun` e um job com:

```
session_id, agent_id, model_ref, permission_set, tools_snapshot, prompt, cancellation_token
```

Orquestracao:

1. scheduler recebe prompt e cria `AgentRun` raiz.
2. subagentes sao `AgentRun` filhos com `parent_id`, `depth` e orcamento proprio.
3. filho publica resultado em `inbox` da sessao; o pai continua sem bloquear.
4. cancellation propaga de pai para filhos.
5. limite de depth impede recursao infinita.

Canal de eventos: `EventBus` com ring buffer lock-free por consumidor. Eventos: `message.delta`, `tool.start`, `tool.end`, `agent.done`, `compaction.done`, `error`.

## Memoria

### Por request

`std::pmr::monotonic_buffer_resource` ligado a um buffer de arena. Todo objeto temporario do request (JSON parse, prompt montado, mensagens LLM) usa esse allocator. No fim do request, a arena e descartada de uma vez.

### Por sessao

Pool de objetos de longa vida (mensagens, simbolos). Objetos sao liberados de volta ao pool quando a sessao e descarregada do registry.

### Shared memory (milestone 3)

Layout proposto no segmento:

```
[header: version, generation, offsets...]
[hash table de arquivos -> ChunkID + timestamp]
[arena de simbolos: strings internadas + offsets]
```

Leitores (CLI, TUI) mapam somente leitura. Escritor (daemon indexer) usa double-buffer + atomic generation switch para leitores nunca verem estado parcial.

## Seguranca

Modelo de ameaca em 3 linhas:

- **Ativos**: credenciais de provider, conteudo de arquivos do usuario, historico de sessao, tokens de API.
- **Fronteira**:prompt do usuario e conteudo de repositorio sao nao-confiaveis; output de ferramentas e nao-confiavel; configuracao local e semi-confiavel.
- **Atacante plausivel**: prompt injection via arquivo do repo, output malicioso de tool, processo local com acesso ao socket do daemon.

Controles:

- credenciais em keyring/variavel de ambiente, nunca em log, Postgres ou Redis;
- socket do daemon autenticado por token local, permissao 0600;
- sandbox sem rede, sem credenciais e com seccomp/job object obrigatorio;
- comandos de shell exigem aprovacao explicita por sessao;
- path de ferramenta validado contra raiz do workspace;
- sanitizacao de output de ferramenta antes de virar texto no prompt;
- limites de tamanho em todas as entradas vindas de rede/arquivo;
- nenhum segredo em eventos do EventBus.

## Compatibilidade com o upstream

Manter, sempre que possivel, os mesmos nomes conceituais do upstream (`session`, `agent`, `tool`, `instruction`, `compaction`, `boundary`, `keep`) para reduzir friccao de quem ja usa o OpenCode. Nomes de tabelas e campos de protocolo seguem o que ja existe no banco do upstream quando aplicavel.
