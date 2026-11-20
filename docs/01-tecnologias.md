# Tecnologias

Stack proposta para o rewrite em C/C++20/23. Cada item traz o motivo concreto e a alternativa descartada, para evitar escolha por moda.

## Nucleo

| Area | Escolha | Motivo | Alternativa descartada |
| --- | --- | --- | --- |
| Linguagem | C++20/23 | RAII para lifetime de memoria e handle, `std::jthread`, coroutines para I/O assincrono sem callback hell | C puro: exigiria reimplementar abestracoes de container e tipo |
| Build | CMake + Ninja | Padrao, suporta fetch de dependencias e cross-compile | Bazel: custo de configuracao alto para equipe pequena |
| Formato | clang-format (LLVM style com ajuste local) | Consistencia automatica | Formatacao manual |
| Testes | GoogleTest + sanitizers (ASan/UBSan/TSan) | Testes unitarios e deteccao de race no codigo concorrente |adro de teste propr io: custo sem ganho |

## Parsing e indexacao

| Area | Escolha | Motivo |
| --- | --- | --- |
| Parser | tree-sitter (lib C) | Incremental, tolerante a erro, bindings C natural; usado para indexacao e poda de AST |
| Linguagens iniciais | C, C++, Rust, Go, TypeScript, Python | Gramaticas estaveis e cobr em o proprio repo |
| Busca textual | ripgrep como biblioteca (`lib ripgrep`/regex automato) ou implementacao propria com RE2 | Evitar processos externos em cada busca |

RE2 e preferivel a std::regex: std::regex tem complexidade exponencial em padroes patologicos. Se regex literais simples bastarem, usar `string_view::find` e hashed n-gram.

## Persistencia e serializedacao

| Area | Escolha | Motivo |
| --- | --- | --- |
| Banco | SQLite (libsqlite3) | Mesmo modelo do upstream, single file, transacoes, WAL |
| JSON | simdjson (parse) + glue code proprio (write) | Parse rapido com validacao de schema explicita |
| Schema | structs C++ com validacao manual nos limites; sem gerador de codigo na fase 1 | Menos movimento de build; gerador entra se os schemas estabilizarem |
| Cache de tokens | tabela SQLite + hash do prompt | Permite reutilizar prefixo cacheavel do provider |

## Rede e I/O

| Area | Escolha | Motivo |
| --- | --- | --- |
| HTTP/WS client | libcurl + WebSocket (libwebsockets ou curl WS) | Suporte multi-plataforma, proxy e TLS consolidados |
| HTTP server | cpp-httplib ou Boost.Beast | Beast e mais completo para WS; httplib e mais simples para o seed |
| TLS | OpenSSL ou BoringSSL via libcurl | Nao implementar cripto propria |
| Async | io_uring no Linux (opcional), epoll/kqueue/IOCP por plataforma | Abstracao de event loop unica com backend por SO |

Decisao pendente: comecar com blocking I/O + thread pool e introduzir coroutines apenas onde a latencia de stream importar. Isso reduz risco de bugs de lifetime no inicio.

## Concorrencia e memoria

| Area | Escolha | Motivo |
| --- | --- | --- |
| Threads | `std::jthread` + fila MPMC lock-free (Vyukov) | Orquestracao multi-agente com custo conhecido |
| Sincronizacao | mutex/condition_variable por padrao; lock-free so em measurement-proven hotspots | Correcao antes de performance |
| Shared memory | POSIX shm / Win32 file mapping para cache de indice entre processos | Zero-copy entre daemon e CLI |
| Alocadores | arena por request, pool por sessao, PMR (`std::pmr::monotonic_buffer_resource`) | Libera tudo de uma vez no fim do request, sem GC |

Shared memory entre processos e uma feature do milestone 3. A fase 1 usa memoria de processo com estruturas compartilhadas entre threads. Isso evita complexidade de serializacao antes de existir uma medida de latencia que a justifique.

## Compressao de prompt

| Area | Escolha | Motivo |
| --- | --- | --- |
| AST pruning | tree-sitter queries com pesos por no | Remove código irrelevante mantendo assinaturas e imports |
| Sumarizacao | chamada ao proprio provider com modelo pequeno (mesma ideia de compaction do upstream) | Evita modelo local adicional |
| Similaridade | embeddings do provider + busca vetorial local (simples, HNSW so se volume justificar) | Contexto relevante por recencia e similaridade |
| Tokenizacao | contador aproximado local + contagem real do provider quando disponivel | Nao inventar tokenizer proprio |

## Observabilidade

- Logs estruturados em JSON lines, sem PII/tokens.
- Metricas Prometheus ou OpenTelemetry exportaveis via HTTP.
- `tracy` em builds de debug para profiling de alloc e locks.

## Licencas

- OpenCode original: MIT.
- SQLite: public domain.
- tree-sitter: MIT.
- libcurl: MIT-like (curl license).
- simdjson: Apache-2.0.
- GoogleTest: BSD-3.

Verificar licencas de cada dependencia antes de incluir no build final. Nenhuma dependencia AGPL entra no produto.
