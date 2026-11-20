# Compressao vetorial e soft prompts

Este documento registra a intencao de usar soft prompts / vetores de compressao, o que e viavel na pratica e o que esta bloqueado.

## Estado verificado (NAO e suposicao)

Fonte: `https://github.com/ZongqianLi/500xCompressor` e README oficial, lidos em 2026-11-20.

| Fato | Evidencia |
| --- | --- |
| 500xCompressor e um paper ACL 2025 Main | README e citacao `10.18653/v1/2025.acl-long.1219` |
| Comprime ate 500 tokens em 1 token | README, secao Introduction |
| Adiciona 0.3% de parametros ao LLM | README |
| Funciona como LoRA sobre LLaMA-3-8b-Instruct | README, secao Download |
| Os modelos **nao estao abertos ao publico** | README: "Datasets and models were uploaded to Huggingface but are not open to the public" |
| Licenca do repositorio: CC BY 4.0 | README, secao License |
| Retem 62.26-72.89% da capacidade do LLM | README |
| Treinado em Arxiv abstracts e ArxivQA | README |

Conclusao: **nao e possivel usar os pesos do 500xCompressor hoje**. O repositorio publico tem codigo de demo, nao os modelos. Nao ha como baixar os LoRAs.

## Restricao estrutural dos providers fechados

Mesmo com os pesos disponiveis, OpenAI, Anthropic e Google aceitam **texto**, nao vetores de input. Soft prompts exigem acesso aos embeddings/estados internos do modelo. Isso so funciona com modelo local open-weight servido por llama.cpp/vLLM.

Portanto ha duas trilhas:

### Trilha A — compressao vetorial real (bloqueada hoje)

Requisitos para desbloquear:

1. obter os LoRAs do 500xCompressor (hoje nao publicos), **ou** treinar compressao propria sobre Llama/Qwen com infra propr ia;
2. servir o modelo localmente com llama.cpp ou vLLM expondo injecao de embeddings;
3. implementar o compressors em C++ (o repo de referencia e Python);
4. medir retencao de capacidade em tarefas de codigo — o paper mede QA de abstracts, nao codigo.

Custo: GPU, tempo de treino, e risco de perder 30% da capacidade em tarefas que exigem exatidao de codigo.

### Trilha B — compressao em texto + prompt caching (viavel agora)

O que ja esta no plano:

- DSL compacta determinstica (`docs/02-contexto.md`, secao 7), com retencao minima de 70% por classe;
- compactacao por resumo local com `keep` e janela nativa;
- prompt caching do provider com prefixo estavel;
- poda de AST com tree-sitter.

Essa trilha entrega reducao de tokens mensuravel sem depender de pesos indisponiveis nem de modelo local.

## Alternativa mais promissora: Boa Compressor (fato verificado)

Paper `2610.33006`, "Linear-Time Context Compression with State-Space Encoding". Compressor online baseado em Mamba, uma unica passada linear sobre contexto de qualquer tamanho, destilado do logit do modelo completo. Ate **16x** compressao e uma ordem de magnitude mais rapido que compressores baseados em Transformer — roda por query, nao so offline.

**Inferencia**: resolve o problema que derrubou 500xCompressor (latencia de compressao e pesos fechados). Continua exigindo modelo local com injecao de representacao. Passa a ser a primeira opcao da Trilha A; 500xCompressor fica como referencia historica.

Outras opcoes verificadas: `LLMLingua-2` (`2403.12968`, extractivo, 3-6x mais rapido, 2-5x compressao) e `EffComp` (`2611.02766`, natureza extractiva evita alucinacao).

## Decisao proposta

Implementar a **Trilha B** como caminho principal e manter a **Trilha A** como modulo opcional com interface abstrata:

```cpp
class ContextCompressor {
public:
  virtual ~ContextCompressor() = default;
  virtual CompressedContext compress(std::span<const Message> history,
                                     TokenBudget budget) = 0;
  virtual std::string name() const = 0;
};
```

Implementacoes:

- `TextDslCompressor` — sempre disponivel;
- `SoftPromptCompressor` — so registrada se um modelo local com suporte a vetores estiver configurado. Se nao estiver, o registro falha com erro explicito, nao fallback silencioso. Primeira implementacao candidata: Boa Compressor; LLMLingua-2 como alternativa extractiva.

Motivo: nao introduzir dependencia de GPU ou modelo indisponivel no build padrao, mas deixar a porta aberta se os pesos aparecerem ou se treinarmos os nossos.

## Prompt caching avancado

Independente da trilha de compressao, implementar:

| Mecanismo | Onde | Efeito |
| --- | --- | --- |
| Prefixo estavel | ordem fixa de instrucoes (`docs/02-contexto.md`) | provider reutiliza KV cache |
| `cache_control` / `cached_content` | adaptador por provider | marca o ultimo bloco **estavel**, nunca o de timestamp |
| hash de prefixo no Redis | `ContextCache` | evita reenviar e recontar tokens |
| TTL por provider | Redis | invalida quando modelo/config muda |
| medicao separada de read/write | tabela `usage` | `cache_read_input_tokens`, `cache_creation_input_tokens`, `input_tokens` |

Fatos oficiais Anthropic (`docs/10-referencias.md`): ordem `tools` -> `system` -> `messages`; lookback de 20 blocos; ate 4 breakpoints; write 5min = 1.25x input, read = 0.1x input (0.05x em alguns modelos); minimo cacheavel de 512 a 4096 tokens dependendo do modelo; entrada so disponivel apos a primeira resposta começar.

Regra: **so marcar como cacheavel o ultimo bloco cujo prefixo e identico entre requests**. Instrucoes que variam por dia/diretorio vao **depois** do breakpoint, nunca antes. Colocar breakpoint em bloco com timestamp gera miss permanente.

## Invariantes adicionais

- Nenhum modulo de compressao vetorial entra no build sem modelo local configurado e validado.
- Toda economia de tokens e measures antes e depois, com contagem do provider quando disponivel.
- Perda de capacidade e medida por acerto em tarefas de codigo, nao apenas por ratio de compressao.
- Nao substituir conteudo que exige exatidao (diffs, stack traces, codigo do arquivo aberto) por vetor.
