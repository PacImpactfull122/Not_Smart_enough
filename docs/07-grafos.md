# Grafo de conhecimento ("cerebro com nodes")

Subsistema que guarda entidades e relacoes da sessao como grafo persistente, permitindo ao agente navegar contexto por conexao em vez de apenas por texto.

## Modelo de ameaca em 3 linhas

- **Ativos**: conhecimento derivado do repo, memorias da sessao, relacoes entre simbolos.
- **Fronteira**: conteudo gerado pelo modelo e nao-confiavel; simbolos vem de codigo nao-confiavel.
- **Atacante plausivel**: prompt injection que cria nos falsos para influenciar respostas futuras.

## Escolha: Postgres com tabelas `node` e `edge`

Motivo: ja e a fonte de verdade do projeto, tem JSONB e transacoes. Evita um quinto servico (Neo4j). Se a traversia ficar lenta ou precisar de Cypher, migrar depois com export.

```sql
node(
  id           bigserial primary key,
  session_id   uuid not null,
  kind         text not null,      -- file | symbol | decision | test | note | sandbox_result
  key          text not null,      -- chave estavel dentro da sessao
  payload      jsonb not null,      -- inclui acceptance: condicao de aceite
  embedding    bytea,              -- opcional, vetor binario
  source_seq   bigint,             -- mensagem de origem
  confidence   real not null default 1,
  created_at   timestamptz not null,
  unique(session_id, kind, key)
)

edge(
  src_id       bigint references node(id),
  dst_id       bigint references node(id),
  relation     text not null,      -- defines | calls | imports | derives_from | refutes | depends_on
  weight       real not null default 1,
  source_seq   bigint,
  primary key(src_id, dst_id, relation)
)
```

Indices: `(session_id, kind, key)`, `edge(src_id)`, `edge(dst_id)`, GIN em `payload`.

## Regras

1. **Proveniencia obrigatoria**: todo no carrega `source_seq`. No sem origem e recusado.
2. **Append-only**: correcoes criam novo no com `key` versionada; o antigo recebe edge `refutes`.
3. **Confianca**: nos derivados de output de ferramenta ou inferencia do modelo comecam com confianca menor que nos derivados de codigo analisado por tree-sitter.
4. **Limites**: maximo de nos por sessao e de profundidade de traversia, para evitar explosion.
5. **Validacao de chave**: `key` segue allowlist por `kind`; nunca interpola entrada do modelo em SQL — usar parametrizacao sempre.

## Consultas tipicas

- simbolos que chamam um dado simbolo: `edge where dst_id = ? and relation = 'calls'`
- decisoes que derivam de um teste: traversia `derives_from`
- contexto relevante para um prompt: nos com `embedding` proximo + nos a 1-2 saltos dos nos do arquivo atual

## Ferramentas expostas

| Ferramenta | Entrada | Saida |
| --- | --- | --- |
| `graph.upsert` | kind, key, payload, source_seq | no criado ou existente |
| `graph.link` | src, dst, relation, weight | edge criada |
| `graph.neighbors` | no, relation, depth | lista de nos |
| `graph.search` | termo ou vetor, limite | nos ranqueados |

O snapshot de tools segue permissao efetiva. Agentes `plan` recebem `graph.neighbors` e `graph.search`, nao escrita.

## Integracao com contexto

O grafo alimenta a secao `I` e `W` da DSL compacta (`docs/02-contexto.md`): nos viram IDs internos e referencias por hash, nunca texto longo repetido.

## Verificacao

- teste de que todo no tem `source_seq` valido;
- teste de que `graph.upsert` nao permite SQL injection via `key`;
- teste de que traversia com depth 5 termina em tempo limitado;
- teste de que edge `refutes` mantem o no antigo intacto.
