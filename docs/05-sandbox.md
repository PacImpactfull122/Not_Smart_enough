# Sandbox de raciocinio

Subsistema onde a propria IA executa codigo para pensar: matematica, testes, contas e exploracao controlada. O resultado volta como tool result, nao como estado interno mutavel do modelo.

## Modelo de ameaca em 3 linhas

- **Ativos**: arquivos do usuario, credenciais de provider, historico, rede local.
- **Fronteira**: codigo gerado pelo modelo e nao-confiavel, mesmo quando o prompt parece inofensivo.
- **Atacante plausivel**: prompt injection via arquivo do repo que induz o agente a exfiltrar dados ou destruir arquivos.

## Isolamento

Camadas, aplicadas em ordem:

1. **Processo separado** por execucao, com `fork`/`posix_spawn` no Linux e `CreateProcess` no Windows.
2. **Usuario sem privilegio** (`nobody` ou uid dedicado), sem acesso a credenciais.
3. **Filesystem**: diretorio de trabalho temporario, montado somente leitura para o repo; escrita permitida so em `/sandbox/out`.
4. **Recursos**: `setrlimit` para CPU, memoria, tamanho de arquivo e numero de processos.
5. **Syscall**: seccomp-BPF no Linux permitindo so leitura/escrita de FDs, memoria e exit. Sem `socket`, sem `execve` aninhado.
6. **Rede**: desligada por padrao. Nenhum acesso a provider de dentro do sandbox.
7. **Tempo**: timeout duro por execucao, com kill do grupo de processos.

No Windows, usar Job Objects + restricted token. Se uma camada nao estiver disponivel na plataforma, a execucao e recusada: nao degraded silencioso.

## Linguagens aceitas

- **C e C++**: compiladas com o toolchain do projeto, dentro do sandbox.
- **Expressoes matematicas**: avaliadas por um interpretador proprio pequeno, sem `eval` de linguagem externa.
- **Testes**: execucao de binarios de teste ja compilados pelo projeto.

Python fica fora: a restricao do projeto e C/C++/HTML/CSS. Se o modelo pedir Python, a ferramenta responde com erro explicito, nao tenta instalar nada.

## Memoria de trabalho

Cada sessao tem um `Workspace` persistente no Postgres:

```
workspace(
  session_id,
  slot_key,
  kind,        -- note | hypothesis | calculation | test_result
  content,     -- JSONB
  created_seq, -- seq da mensagem que originou
  ttl
)
```

Regras:

- entradas sao append-only; correcoes criam novo slot com chave versionada;
- leitura por `slot_key` e por faixa de `created_seq`;
- limite de slots por sessao evita crescimento infinito;
- conteudo e validado por tamanho e tipo no servidor, nunca confiado do cliente.

O sandbox e stateless: toda memoria persistente vai para o `Workspace`, nunca para dentro do processo isolado.

## Ferramentas expostas ao agente

| Ferramenta | Entrada | Saida |
| --- | --- | --- |
| `sandbox.run` | linguagem, codigo, stdin, limits | stdout, stderr, exit_code, duration_ms |
| `sandbox.calc` | expressao | valor numerico ou erro de parsing |
| `sandbox.test` | alvo, filtro | resultados de teste |
| `workspace.write` | slot_key, kind, content | confirmacao |
| `workspace.read` | slot_key ou faixa | entradas |

O snapshot de tools segue a mesma regra do resto do sistema: `sandbox.run` exige permissao de `execute` efetiva na sessao. Agentes em modo `plan` recebem so `sandbox.calc` e `workspace.read`.

## Resultado da execucao

O tool result e truncado por limite configuravel, com cabeçalho indicando:

```
exit_code, duration_ms, bytes_omitted
```

Nunca incluir paths absolutos do host, variaveis de ambiente ou credenciais. Erros de compilacao sao sanitizados antes de entrar no prompt.

## Verificacao

- teste de que um programa tentando `open("/etc/passwd")` falha;
- teste de que `socket()` e bloqueado por seccomp no Linux;
- teste de que exceder `RLIMIT_AS` termina o processo sem matar o daemon;
- teste de que nenhum arquivo fora de `/sandbox/out` foi criado;
- teste de que `sandbox.run` nao aparece no snapshot de um agente `plan`.
