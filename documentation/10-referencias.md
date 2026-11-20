# Referencias verificadas

Pesquisa feita em 2026-11-20 via arXiv API, GitHub API e documentacao oficial Anthropic. Websearch estava indisponivel (HTTP 402), então todas as fontes abaixo vieram de fetch direto.

Convencao: **fato verificado** = lido na fonte; **inferencia** = conclusao nossa a partir do fato.

## Compressao de prompt

### 500xCompressor (fato verificado)

- Paper ACL 2025 Main, `10.18653/v1/2025.acl-long.1219`, repo `ZongqianLi/500xCompressor`.
- Comprime 500 tokens em 1-16 tokens; 0.3% de parametros extras; LoRA sobre LLaMA-3-8b-Instruct.
- README: "Datasets and models were uploaded to Huggingface but are not open to the public".
- Licenca CC BY 4.0 no repo, **sem licenca declarada** no campo `license` da API do GitHub.
- Retem 62.26-72.89% da capacidade do LLM.
- Treinado em Arxiv abstracts + ArxivQA, nao em codigo.

**Inferencia**: inutilizavel hoje. Mesmo com pesos, perda de ~30% da capacidade e dominio diferente do nosso caso.

### LLMLingua / LongLLMLingua / LLMLingua-2 (fato verificado)

| Paper | ID arXiv | Resultado declarado |
| --- | --- | --- |
| LLMLingua | `2310.05736` (EMNLP 2023) | ate 20x compressao com pouca perda; budget controller + compressao iterativa |
| LongLLMLingua | `2310.06839` (ACL 2024) | ate 21.4% de ganho com ~4x menos tokens; 1.4-2.6x menos latencia em 2x-6x |
| LLMLingua-2 | `2403.12968` (Findings ACL 2024) | compressao extractiva via classificacao de tokens; 3-6x mais rapido que metodos anteriores; 1.6-2.9x menos latencia fim a fim |

**Inferencia**: LLMLingua-2 e a opcao mais adequada para compressao extractiva no cliente, porque nao gera tokens alucinados e roda com modelo pequeno. Porem adiciona dependencia de modelo.

### Boa Compressor (fato verificado)

- `2610.33006`, "Linear-Time Context Compression with State-Space Encoding".
- Compressor online baseado em Mamba, uma passada linear, destilacao do logit do modelo completo.
- Ate 16x compressao, uma ordem de magnitude mais rapido que compressores Transformer; roda por query, nao so offline.

**Inferencia**: alternativa mais promissora que 500xCompressor para o modulo vetorial, porque resolve o problema de latencia de compressao. Continua exigindo modelo local com injecao de representacao.

### EffComp (fato verificado)

- `2611.02766`, AACL-IJCNLP 2026. Reranker BERT + seletor de sentencas, natureza extractiva evita alucinacao, treino hibrido RL + supervised.

### PC-SubMax (fato verificado)

- `2609.32474`. Compressao seletiva como maximizacao submodular regularizada com restricao de knapsack; garantia approximada `>= 1/2` do otimo; usa encoder, sem scoring autoregressivo.

### Linguistic rules (fato verificado)

- `2607.25335`, EMNLP 2026. Compressor determinstico baseado em regras linguisticas, **sem forward pass do LM** na compressao, so CPU. Performance similar a metodos avancados em compressao leve-moderada; degrada em compressao agressiva.

**Inferencia**: e o respaldo Academico mais proximo da nossa DSL. Reforca que compressao determinstica em CPU e viavel, mas so ate certo ponto.

### GARP (fato verificado)

- `2611.11064`. Agentes generativos em Godot, modelo open-weight 3B local, sem cloud. Usa multi-tier caching, prompt compression e execucao assincrona. 25 de 27 cenarios passaram.

**Inferencia**: valida a arquitetura de grafo + cache + compressao + modelo local em ambiente com restricoes de hardware.

## Riscos da compressao (fato verificado)

### CompressAgent (fato verificado)

- `2608.01056`, "Control Under Compression: Reliability Frontiers for Tool-Using Agents".
- 15.525 execucoes, 9 contextos de controle, 3 modelos Qwen, 6 orcamentos.
- Com 75% do contexto retido: rewriting 92.7%, section-based 92.4%, baseline completo 93.8%.
- Entre 50% e 35% os metodos divergem: a 35%, section-based 47.0%, obligation-aware 39.0%, generic rewriting 19.9%.
- De 25% a 10%, protocolos executaveis ficam frageis.
- Falhas aparecem como erros de execucao de tool e parsing de acao.

**Correcao imposta ao projeto**: nossa meta "menos de 70% dos tokens" e ambigua. Interpretada como **reter 70%**, esta na zona segura. Interpretada como **reducao de 70% (reter 30%)**, esta na zona de colapso. Vou redefinir: retencao minima de 70% para contexto de controle de ferramentas, com qualificacao por contexto.

### Referential Dangling (fato verificado)

- `2608.04569`. Compressao hard (selecao independente de tokens/sentencas) separa pares de evidencia dependentes.
- A ratio 0.30, Beaver deixa caminho de resposta incompleto em 34-54% dos exemplos bridge.
- Reinserir o par de apoio removendo outros para manter orcamento melhora acerto em 29-34 pontos.
- Classificador compacto recupera 4.7 pontos mudando ratio so de 0.30 para 0.31.

**Correcao imposta**: compressao deve otimizar relevancia **e completude referencial**. Nossa DSL precisa manter pares simbolo-definicao juntos, nao so ranquear por relevancia.

### Prompt minimization blackbox (fato verificado)

- `2609.36289`. Few-shots reduzidos em media 65.3% +- 15.8% em caracteres preservando fidelidade proposicional. Modelos preferem reter identificadores logicos e declaracoes de restricao, descartando prosa.

**Inferencia**: corrobora a DSL: chaves curtas e restricoes sao o que importa, prosa e o que pode sair.

### Diffusion-aware warning (fato verificado)

- `2605.17932`. LLMLingua-2 em LLaDA-8B: compressao ~50% degrada bastante raciocinio matematico apesar de alta similaridade semantica. Falhas por omissao de informacao, nao drift.

**Correcao imposta**: nunca comprimir prompt de matematica/simulacao com perda. O ambiente 3D recebe spec completa.

## Memoria e esquecimento

### Survey de compressao centrada em memoria (fato verificado)

- `2610.21165`. Reformula compressao de contexto como gerenciamento de memoria: distingue `compute-active` de `compute-inactive`, e define transicoes de estado (write, offload, recall, retrieval-and-injection, materialization, eviction, consolidation, forgetting) com modelo de custo orcamentario.

**Inferencia**: nossa separacao DSL (camada estavel) + Postgres (inativo) + Redis (cache) segue essa distincao. Podemos adotar a vocabularia no doc de contexto.

### Forgetting Without Restarting (fato verificado)

- `2609.04875`. Apagar um registro plaintext de memoria **nao** remove artefatos derivados: resumos, plano de ferramentas e KV cache permanecem.
- "Instruction-based forgetting collapses under elicitation (Leak@probes = 1.00)".
- Provenance-Guided Selective Replay atinge esquecimento exato com ate 9x menos tokens recomputados que reset completo.

**Correcao imposta**: nosso edge `refutes` so marca a correcao; precisa de replay seletivo a partir do `source_seq` para realmente apagar a influencia do dado revogado. Implementar `graph.forget` com replay, nao so update.

### SkillZip (fato verificado)

- `2608.11079`. Compressao de skills sem avaliacao: "explain once, reference many", objetivo de minimum description length com restricao dura de cobertura, modo Zip-on-Write.

**Inferencia**: bom modelo para comprimir catalogo de tools e instrucoes repetidas na camada estavel da DSL.

## Multi-agente

### Aligning AI Teams (fato verificado)

- `2611.17820`. Alignment de agente individual nao transfere para time. `diffusion of responsibility` explica parte da diferenca. Critica mutua e group chat **nao** restauram confiabilidade. Designated safety lead e coordenador publicando spec de seguranca **funcionam**.

**Correcao imposta**: nosso plano mestre nao tem safety lead. Adicionar agente com papel exclusivo de seguranca, com autoridade para bloquear acoes.

### WorkflowArena (fato verificado)

- `2611.12856`. 145 tarefas longas em 8 dominios, incluindo matematica formal e producao 3D. Organizacao do workflow e fator mensuravel de desempenho: workflows autorados por Claude Opus 5.5 melhoram executores mais fracos. Estrategias de delegacao diferem por familia de modelo.

**Inferencia**: justifica investir em orquestracao antes de trocar de modelo. Nosso scheduler deve registrar delegacoes para medicao.

### Dittany (fato verificado)

- `2610.42144`. Reparo guiado por contribuicao em MAS: grafo de contribuicoes com condicoes de aceite. Detecta falhas com 88.28% precision / 79.18% recall, melhora acuracia em 26.74 pontos sobre execucao nativa, custo de tokens 17.92% acima do nativo.

**Inferencia**: nosso grafo `node`/`edge` pode servir como grafo de contribuicoes, nao so de conhecimento. Adicionar `acceptance` no payload.

### Root-cause analysis multi-agente (fato verificado)

- `2611.23294`. Especialistas em paralelo com tecnicas distintas (fault tree, Kepner-Tregoe, call-chain); hipoteses alcancadas independentemente por varios analistas recebem ranking maior. Suspeita inicial do ticket vai para analista isolado para nao ancorar o time.

**Inferencia**: padrao util para o agente `review`: hipoteses independentes primeiro, votacao depois, e isolar a hipotese do usuario para evitar ancoragem.

### katIA-MA (fato verificado)

- `2611.10958`. Analise de impacto com 3 agentes especializados: F1 29.1% (FQN) vs 22.4% do prompt unico. Custo: 0.570s -> 11.214s e 1.235 -> 6.462 tokens.

**Inferencia**: quantifica o trade-off. Multi-agente melhora resultado, mas custa ~5x tokens e ~20x tempo. Deve ser opt-in por tarefa.

### OverServe (fato verificado)

- `2611.14808`. Ataque de amplificacao de recursos em agentes SaaS via co-design tool-prompt. Cada tool call termina normalmente, mas o agente repete checagem de condicao inacabada. Amplificacao maxima de 93.88x em requisicoes de API, passando nos checks originais.

**Correcao imposta**: precisamos de limite duro de tool calls por turn e detector de loops de checagem, nao so timeout por execucao.

### ICAE-Bench (fato verificado)

- `2607.21217`. Benchmark para agentes de codificacao em construcao interativa de projetos a partir de requisito fuzzy, com User Agent e testes black-box.

**Inferencia**: bom candidato a benchmark de aceite da Fase 7, no lugar de apenas "subset de testes do upstream".

### Mind the Gap (fato verificado)

- `2611.23142`. Sistema multi-agente deployado para manutencao de documentacao: 43 dias, 499 tickets, 92 gaps. 81.1% das assercoes verificaveis corretas. Depois do reparo, juizes cegos preferem a nova resposta em 71.4% dos casos.

**Inferencia**: valida o feedback via grafo (`refutes` + reparo) com medida real de melhora.

## Prompt caching (fato verificado, fonte oficial Anthropic)

Fonte: `https://platform.claude.com/docs/en/build-with-claude/prompt-caching`.

- Ordem de cache: `tools` -> `system` -> `messages`.
- `cache_control` no ultimo bloco **estavel**. Colocar em bloco com timestamp gera cache miss permanente.
- Lookback de 20 blocos; se breakpoint avancar mais que isso sem write, perde hit.
- Ate 4 breakpoints explicitos.
- TTL default 5 min; 1h custa o dobro do input base.
- Precos: write 5min = 1.25x input, write 1h = 2x, read = 0.1x (0.05x em alguns modelos, 0.025x em outros).
- Minimo cacheavel por modelo: 512 tokens (modelos novos), 1024, 2048 ou 4096 dependendo do modelo.
- Metrica: `cache_creation_input_tokens`, `cache_read_input_tokens`, `input_tokens`. Total = soma dos tres.
- Entrada disponivel somente apos primeira resposta comecar; requests paralelos precisam esperar o primeiro.
- `cache_control` no topo (automatic caching) move o breakpoint sozinho; em Bedrock legacy, nao suportado.

**Correcao imposta**: nosso doc de contexto diz que data/ambiente variam e ficam no fim — correto. Mas precisamos de **breakpoint explicito no ultimo bloco estavel**, nao so ordem. E `usage` deve registrar `cache_read_input_tokens` separadamente.

## Seguranca de sandbox (inferencia com suporte indireto)

Nao encontrei paper primario especifico para seccomp em agentes na busca feita. Os controles do `docs/05-sandbox.md` permanecem como decisao de engenharia, nao como fato de literatura. OverServe (`2611.14808`) reforca a necessidade de limite de chamadas.

## Resumo de correcoes aplicadas aos demais docs

1. Meta de compressao redefinida: **retencao minima de 70%**, nunca "reducao de 70%".
2. DSL deve manter pares de evidencia (referential dangling).
3. `graph.forget` com replay seletivo, nao so `refutes`.
4. Agente `safety` com autoridade de bloqueio.
5. Limite duro de tool calls por turn + detector de loop.
6. Breakpoint explicito de cache + metricas separadas de read/write.
7. Modulo vetorial: Boa Compressor passa a ser alternativa preferida ao 500xCompressor.
8. Benchmark de aceite: ICAE-Bench para tarefas interativas de codigo.
