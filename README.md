# 🎮 CodleC - Jogo estilo Wordle em C

**CodleC** é uma versão em C do famoso jogo de palavras "Wordle", com suporte a múltiplas dificuldades, dicas, feedback visual com cores no terminal e persistência de resultados. O jogo é completamente jogável em modo texto e multiplataforma (Linux/macOS e Windows).

---

## 🧠 Objetivo

Descubra a palavra secreta de 5 letras em um número limitado de tentativas. Use o feedback por cores para guiar seus palpites e, se necessário, peça dicas!

---

## ✨ Funcionalidades

- Modos de dificuldade: Fácil, Médio, Difícil e Demo (palavra fixa).
- Suporte a dicas limitadas por partida com sistema de cooldown.
- Interface colorida via ANSI escape codes (para terminais compatíveis).
- Teclado virtual com feedback colorido.
- Histórico de resultados salvo em formato JSON (`resultados.json`).
- Palavras carregadas dinamicamente de arquivos `.txt`.

---

## 🗂️ Estrutura de Arquivos

- `main.c` – Código principal do jogo.
- `palavras.txt` – Lista de palavras comuns (modo Fácil/Médio).
- `palavras_dificeis.txt` – Lista de palavras para o modo Difícil.
- `resultados.json` – Arquivo gerado automaticamente com os resultados.

---

## 🧰 Requisitos

- Compilador C (GCC, Clang ou MSVC).
- Terminal que suporte códigos ANSI para cores (em Unix, padrão; no Windows, é configurado automaticamente no código).

---

## 🔧 Compilação

### Linux/macOS

```bash
gcc main.c -o codlec
```

### Windows (usando MinGW)

```bash
gcc main.c -o codlec.exe
```

> Dica: certifique-se de que os arquivos `palavras.txt` e `palavras_dificeis.txt` estejam no mesmo diretório do executável.

---

## ▶️ Execução

```bash
./codlec       # Linux/macOS
codlec.exe     # Windows
```

---

## 📊 Resultados

Ao vencer uma partida, você pode optar por salvar seu resultado. As estatísticas são armazenadas em `resultados.json`, incluindo:

- Palavra adivinhada.
- Número de tentativas.
- Dificuldade do jogo.

---

## 📚 Como Jogar

Durante o jogo:

- Digite uma palavra com 5 letras e pressione Enter.
- Use `H` para pedir uma dica (máx. 4 por jogo, com tempo de espera entre usos).
- Use `P` para pausar a partida.

Cores do feedback:

- 🟩 **Verde**: letra correta na posição correta.
- 🟨 **Amarelo**: letra correta na posição errada.
- ⬜ **Cinza**: letra não está na palavra.

---

## 🧪 Modo Demo

Escolha a opção **Demo** no menu de dificuldades para testar o jogo com a palavra fixa **"TESTE"**. Útil para depuração ou demonstrações.

---

## 📌 Observações

- Apenas palavras válidas da lista são aceitas.
- É necessário que `palavras.txt` tenha pelo menos 100 palavras e `palavras_dificeis.txt` pelo menos 10.
- O jogo usa funções específicas por sistema para entrada via teclado (Windows: `_getch`; Unix: `termios`).

---

## 📝 Licença

Projeto educacional para fins de aprendizado de programação em C.

---

## 📬Publicações

- [Paper](https://dev.to/arthur2023102413/codlec-building-a-wordle-clone-in-c-2e5n)
- [Dev.to](https://dev.to/arthur2023102413/codlec-building-a-wordle-clone-in-c-2e5n)
- [Medium](https://medium.com/@2023102413/codlec-como-desenvolvi-um-jogo-de-palavras-em-c-993a16532b48) 

## Para saber mais siga meu LinkedIn:

### [🪪 LinkedIn](https://www.linkedin.com/in/arthurdamiao/)
