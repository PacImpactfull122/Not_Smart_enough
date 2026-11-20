# Ambiente de matematica 3D e fisica

Ambiente proprio onde a IA testa hipoteses com geometria e simulacao fisica, separado do sandbox de codigo (`docs/05-sandbox.md`).

## Motivo

Matematica 3D e fisica exigem observacao de resultado, nao so execucao de funcao. O agente precisa montar cena, aplicar forcas, medir e comparar com uma expectativa. Isso e uma ferramenta de raciocinio, com o mesmo modelo de ameaca do sandbox.

## Motor

Escolha proposta: **Bullet Physics** (C++, licenca zlib).

Alternativas:
- **Jolt Physics** (C++, permissiva): mais novo, alto desempenho; avaliar se APIs estaveis o suficiente.
- **motor proprio**: integrador semi-implicito Euler/Verlet, colisoes esfera/caixa/plano. Sem dependencia, mas limitado.

Decisao pendente: comecar com Bullet; trocar para Jolt se o custo de build ou a API incomodar.

## Arquitetura

```
agente --(scene spec JSON)--> sim_runner --(bullet)--> medicos
                                  |                      |
                                  +--> UI HTML/CSS <-----+
```

`sim_runner` roda em processo separado, com as mesmas camadas de isolamento do sandbox (`docs/05-sandbox.md`): usuario sem privilegio, rlimit, seccomp, sem rede, timeout duro.

## Formato de entrada

```json
{
  "scene": [
    {"id": "bola", "shape": "sphere", "radius": 0.5, "mass": 1, "pos": [0, 5, 0]},
    {"id": "chao", "shape": "plane", "pos": [0, 0, 0]}
  ],
  "forces": [{"target": "bola", "type": "gravity", "value": [0, -9.81, 0]}],
  "steps": 600,
  "dt": 0.0167,
  "probes": [
    {"at": 120, "target": "bola", "field": "position"},
    {"at": 600, "target": "bola", "field": "velocity"}
  ]
}
```

Validacao no servidor por allowlist de `shape`, `type` de forca e `field`. Nenhum campo arbitrario passa para o motor.

## Saida

- series temporais dos `probes` em JSON;
- energia cinetica/potencial por passo, quando solicitado;
- violacoes de restricao (penetracao, explosao numerica);
- hash da cena + configuracao do motor, para reproducibilidade.

## Visualizacao

A UI HTML/CSS puro (`docs/01-tecnologias.md`) mostra:

- lista de cenas e parametros;
- grafico de series em SVG gerado no servidor (sem JS);
- estado final em tabela;
- link para o JSON bruto.

Nao ha canvas interativo em tempo real sem JS. Isso e uma consequencia da restricao de linguagem, nao um detalhe de implementacao. Se interatividade virar requisito, a alternativa e renderizar quadros como PNG no servidor e servir via SSE.

## Ferramentas expostas

| Ferramenta | Entrada | Saida |
| --- | --- | --- |
| `sim.run` | scene spec | medicos + hash |
| `sim.compare` | hash A, hash B, tolerancia | diferencas |
| `sim.probe` | cena, campo, passos | serie temporal |

So agentes com permissao de `execute` recebem `sim.run`. `plan` recebe `sim.probe` sobre cenas ja executadas.

## Casos de uso

- verificar conservacao de energia em colisao;
- testar solucao analitica de trajetoria contra simulacao;
- medir erro de integracao com diferentes `dt`;
- validar restricoes de um mecanismo antes de escrever codigo no repo.

## Verificacao

- comparar queda livre com solucao analitica `y = y0 - 0.5*g*t^2` dentro de tolerancia conhecida;
- teste de que cena com campo desconhecido e recusada;
- teste de que `sim.run` nao tem acesso a rede;
- teste de que duas execucoes do mesmo hash produzem a mesma saida.
