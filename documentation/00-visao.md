# Visao geral

## Objetivo

Reescrever o OpenCode em C e C++ (C++20/23), eliminando o runtime TypeScript/Bun. O resultado deve ser uma ferramenta de codificacao assistida por IA com startup rapido, pegada de memoria pequena e controle explicito de paralelismo e alocacao.

Este repositorio comeca vazio. A referencia de comportamento e o projeto oficial `anomalyco/opencode` (branch `dev`, MIT), que serve como especificacao funcional, nao como codigo a portar linha a linha. As decisoes de compressao, memoria e orquestracao sao ancoradas em papers e documentacao oficial listados em `documentation/10-referencias.md`.

## O que e inspiracao e o que e mudanca

Inspirado no upstream (ver /tmp/opencode-ref, clonado para leitura):

- Sessoes como unidade de estado, com mensagens persistidas e numeradas (`seq`).
- Agentes com permissoes proprias (`build`, `plan`, `general`, `title`, `compact`).
- Contexto montado em duas fases: `select` (sessao, agente, instrucoes, tools) e `load` (modelo e historico).
- Ferramentas com snapshot por permissao, nao um registro global fixo.
- Compactacao de contexto com resumo local, janela nativa do provider e `keep` de mensagens recentes.
- Instancias por `location` (diretorio/workspace), com servicos escopados.

Mudancas propostas neste rewrite:

- Orquestracao multi-agente com threads nativas e filas de trabalho, em vez de fibers/Effect.
- Estado compartilhado em memoria com estruturas lock-free onde o padrao de acesso justificar; PostgreSQL como fonte de verdade e Redis como cache/fila.
- Alocadores arena/pool por request e por sessao, com lifetime explicito.
- Sandbox de execucao para a propria IA pensar: matematica, testes e contas rodam isolados e o resultado volta como ferramenta.
- Compressao de contexto em duas trilhas: DSL compacta determinstica (implementavel agora, com **retencao minima de 70% por classe de conteudo**, ver `documentation/02-contexto.md`) e soft prompts vetoriais (modulo opcional, ver `documentation/06-compressao-vetorial.md`).
- Grafo de conhecimento persistente conectando simbolos, decisoes, testes e resultados (`documentation/07-grafos.md`).
- Ambiente de matematica 3D e fisica com simulacao real para testar hipoteses (`docs/08-ambiente-3d.md`).
- Indexacao de codigo nativa com tree-sitter, reaproveitada pela poda e pelo sandbox.
- Interface HTML/CSS puro servida pelo daemon C++, sem JavaScript no produto.

## Fora de escopo na fase inicial

- Paridade total de UI (TUI e desktop) com o upstream.
- JavaScript em qualquer parte do produto, inclusive plugins.
- Todos os providers de modelos suportados pelo upstream; comecar por OpenAI-compatible e Anthropic.
- MCP completo no primeiro milestone.
- Uso dos pesos do 500xCompressor no build padrao: nao estao publicos. Ver `documentation/06-compressao-vetorial.md`.

## Criterio de sucesso do milestone 1

Executar `opencode-cpp run "explique este arquivo"` em um repositorio local, com:
1. indexacao tree-sitter do diretorio,
2. montagem de contexto com instrucoes e tools,
3. chamada a um provider compativel,
4. resposta streamada no terminal com custo de memoria deterministico por request.

## Premissas assumidas nesta revisao

As seguintes decisoes foram assumidas porque a ferramenta de perguntas nao estava disponivel. Se alguma estiver errada, corrigir antes de seguir.

1. **Contexto compacto**: DSL propria, serializada de forma determinstica, com chaves curtas e referencias por ID. Reversivel e depuravel.
2. **Soft prompts**: modelo open-weight local (llama.cpp/vLLM) como unica via viavel para vetores; provider fechado recebe texto.
3. **Sandbox**: execucao isolada de codigo mais memoria de trabalho persistente no mesmo subsistema.
4. **Banco**: PostgreSQL fonte de verdade (inclusive grafo `node`/`edge`), Redis cache/fila/lock, JSONB nos payloads.
5. **Interface**: HTML/CSS puro servido pelo daemon C++, sem JavaScript, interacao por forms e SSE.
6. **Fisica 3D**: Bullet Physics como primeiro motor, com Jolt como alternativa.
