# Visao geral

## Objetivo

Reescrever o OpenCode em C e C++ (C++20/23), eliminando o runtime TypeScript/Bun. O resultado deve ser uma ferramenta de codificacao assistida por IA com startup rapido, pegada de memoria pequena e controle explicito de paralelismo e alocacao.

Este repositorio comeca vazio. A unica referencia de comportamento e o projeto oficial `anomalyco/opencode` (branch `dev`, MIT), que serve como especificacao funcional, nao como codigo a portar linha a linha.

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
- Estado compartilhado em memoria com estruturas lock-free onde o padrao de acesso justificar; persistencia em SQLite como fonte de verdade.
- Alocadores arena/pool por request e por sessao, com lifetime explicito.
- Prompt compression com poda de AST via tree-sitter, integrada ao pipeline de contexto.
- Indexacao de codigo nativa, reaproveitando a mesma arvore sintatica usada pela poda.

## Fora de escopo na fase inicial

- Paridade total de UI (TUI e desktop) com o upstream.
- Compatibilidade de plugins escritos em TypeScript/JavaScript.
- Todos os providers de modelos suportados pelo upstream; comecar por OpenAI-compatible e Anthropic.
- MCP completo no primeiro milestone.

## Criterio de sucesso do milestone 1

Executar `opencode-cpp run "explique este arquivo"` em um repositorio local, com:
1. indexacao tree-sitter do diretorio,
2. montagem de contexto com instrucoes e tools,
3. chamada a um provider compativel,
4. resposta streamada no terminal com custo de memoria deterministico por request.
