# Ambiente de desenvolvimento e limitacoes verificadas

Estado verificado em 2026-11-20 nesta maquina. Nada aqui e estimativa.

## Compiladores e testes

| Item | Comando | Resultado |
| --- | --- | --- |
| clang++ | `clang++ --version` | funcional |
| g++ | `g++ --version` | reporta versao, mas falha ao executar `cc1plus` |
| GoogleTest headers | `/usr/include/gtest/gtest.h` | presente |
| GoogleTest lib | `-lgtest` | linka e executa via clang++ |
| ninja | `command -v ninja` | presente |
| make | `command -v make` | presente |

Comando que funciona hoje:

```
clang++ -std=c++20 -Wall -Wextra -Werror -Isrc tests/test_arena.cpp -lgtest -o build/test_arena
./build/test_arena
```

## Bloqueios

| Item | Sintoma | Impacto |
| --- | --- | --- |
| cmake 4.4.4 | `Could not find CMAKE_ROOT`; procura modulos em `/Modules/` | `cmake -S . -B build` falha |
| redis-cli / redis-server | nao encontrados no PATH | nao da para validar cache/fila localmente |
| bullet | `pkg-config --exists bullet` falha | modulo de fisica (Fase 5) nao compila aqui |

## Disponiveis

- `libpq`, `hiredis`, `libcurl`, `tree-sitter` via pkg-config.
- `psql` e servidor `postgres` (`/usr/bin/postgres`).
- `clang-format`.

## Convencao de compilacao enquanto cmake estiver quebrado

Usar clang++ direto para validar codigo novo. O `CMakeLists.txt` existe e deve ser usado assim que o cmake for reinstallado ou tiver `CMAKE_ROOT` correto. Nao declarar "build via cmake verificado" enquanto o comando falhar.

## Proximo passo para desbloquear

Reinstalar cmake com prefixo correto, ou apontar `CMAKE_ROOT` para um diretorio que contenha `Modules/SystemInformation.cmake`. O binario atual foi instalado via AppImage e procura recursos em `/Modules/`, que nao existe.
