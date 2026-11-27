# INTONARUMORI 1.1

Instrumento de harsh noise + ambient/drone para **Old 3DS com Luma3DS** (toque + botões, gravação WAV no SD).

## Gerar o .3dsx
1. Suba **todo o conteúdo desta pasta** (inclusive `.github`) para um repositório GitHub.
2. Aba **Actions** → espere o job *Compilar INTONARUMORI 3DS* ficar verde.
3. Baixe o artefato `INTONARUMORI_1.1_para_cartao_SD`.
4. Copie a pasta `3ds` para a raiz do SD: `SD:/3ds/intonarumori/intonarumori.3dsx` e abra pelo Homebrew Launcher.

## Obrigatório no 3DS
`SD:/3ds/dspfirm.cdc` (Rosalina: L + ↓ + SELECT → Miscellaneous options → Dump DSP firmware). Sem ele o app mostra tela de erro.

## Novidades da 1.1
- **Abas**: HSH · CDX-H · AMB · CDX-A · DRM · WAV · SET. Saiu o H-P (o codex faz o mesmo papel). **CDX-H** carrega o demônio no HARSH; **CDX-A** (antigo A-P, no mesmo estilo do deck) carrega no AMBIENT. D-pad nas duas: ←→ ±1, ↑↓ uma linha da grade. **GRV virou DRM**.
- **O app abre com harsh, ambient e bateria pausados**; ligue cada um no seu botão.
- **Harsh + ambient + bateria ao mesmo tempo.** Cada um tem seu botão de liga/pausa na própria tela (HARSH: ao lado do volume · AMB: ao lado de LEAD · DRM: PLAY/STOP) e seu volume (VOLUME · MASTER · LEVEL). O ícone de pausa do topo continua pausando tudo. Tudo vai para o mix final (e para o REC); o motor desligado não gasta CPU. Escolher um demônio no CDX liga o motor dele. Na tela de cima, **H A D** acendem quando cada motor toca.
- **DRM**: 4 padrões (**P1–P4**), **CHAIN** (toca P1→P4 em sequência, um compasso cada, pulando os vazios), **SAVE/LOAD** (`SD:/3ds/intonarumori/drum.dat`, carrega sozinho ao abrir o app).
- **Mais branco** (rótulos, setas, abas, sigilo e onda) e **efeito VFD segmentado mais forte** (também no pad XY, nos sigilos do codex e na grade da bateria; LITE desliga).
- **Ouvir gravações (WAV)**: escolha o arquivo e toque **PLAY** (ou **B**); toca de novo para parar. A linha mostra o tempo e uma barra de progresso, e o cabeçalho da tela de cima mostra `PLAY mm:ss`. Funciona mesmo com o app pausado, soma por cima do mix, não passa pelo DAMAGE e não entra no REC. Renomear ou apagar o arquivo que está tocando para a reprodução.
- **Bateria** do aparelho no cabeçalho da tela de cima (5 segmentos; CHG quando carregando; pisca em vermelho no último nível).
- Nomes longos na tela de cima não encostam mais no HZ.

## Novidades da 1.0
- **Slider MASTER no AMB**: rodapé da tela de tocar (DRONE e LEAD), mesmo gesto do VOLUME do harsh.
- **Mais volume**: drone ~+9 dB e master ~+4 dB (RMS dos presets de ambient foi de ~0.1 para ~0.2–0.3, pico < 0.8, sem clip).
- **Visual dark NOIZ DEMONS**: preto-esverdeado, fósforo verde, vermelho ritual.
- **SUMMON** (antigo RANDOM, botão ou B no harsh): invoca um dos **30 demônios da Ars Goetia**; o posto/temperamento dele define quanto dos parâmetros é rerolado (Furcas ~15%, Belial 100%). O sigilo aparece 3 s na tela de cima.
- Créditos/licença: `goetia.c/h` vêm do **NOIZ DEMONS** (GPL-3.0). Isso torna o projeto combinado GPL-3.0.

## GROOVE (aba DRM)
Bateria de ruído em 16 passos por cima do harsh ou do ambient.
- **PLAY/STOP** (ou botão A) · **kit** com as setas (ou ←→): ROOM, GATE, MARCH, ARC, ROT, SLAB, DESTROY.
- **Grade** KICK / SNR / HAT / METL: cada toque no passo alterna vazio → x1 → x2 → x3 → x4 (ratchet) → vazio.
- **BPM** (60–200), **SWING**, **LEVEL** · **DICE** (ou B) sorteia um padrão · **CLR** apaga.
- **Humores** (MACH, GRIND, MARCH 7/8, GLTCH, SPIRL, GHOST, SURGE, HUSH): escolher um gera padrão e kit na hora. Com um humor ligado, o **DICE do AMB** segue o mesmo humor (o botão mostra o nome). Toque de novo no humor para voltar ao DICE livre.
- Y (mute) também cala a bateria; PAUSE para tudo.

## Os 30 demônios
- **Harsh**: os 16 presets vão do mais calmo (FURCAS) ao mais caótico, na ordem de "profundidade" dos demônios. **Os sons mudaram de posição**: o preset que abre o app agora é o 16, ASMODEUS. Os demônios sem preset próprio rerolam o som vizinho.
- **Ambient**: demônio com preset do mesmo nome carrega esse preset; os outros 14 têm ambiente próprio (tônica, escala, timbres, modo do lead, reverb). Quanto mais "profundo", mais escuro e agitado.
- A tela de cima mostra o nome e o número (de 30) do demônio, com uma breve interferência na troca (desliga em LITE).

## DAMAGE (aba SET)
**DAMAGE** é o desgaste no mix final (harsh, ambient e bateria). **DMG CHAR** vai de fita (escurece, wow/flutter, quedas que afundam) a digital (menos bits, amostra segurada, cortes). Em 0% o áudio passa intacto.

## HOWL / MIC (HARSH → EDIT → NOISE)
**HOWL** liga um laço que se alimenta sozinho. **HOWL TYPE**: STRING (afinado no PITCH) ou MIC (microfonia numa sala, presa na nota). SUMMON e MUTATE não mexem nesses dois controles.

## Novidades da 0.9
- **Abertura animada** (~5 s, painel VFD de 14 segmentos): teste de lâmpadas, o nome escrito traço a traço com barra de carga âmbar, legenda, versão, crédito "MADE BY MJCDN" digitando, uma pausa com o nome inteiro, feixe de brilho e saída em glitch de harsh noise; a tela de baixo vira um analisador de 16 colunas. **Qualquer botão ou toque pula** (depois de 0,4 s). **O app começa em silêncio (pausado) e só toca quando você aperta PLAY** (ícone na tela, ZL/ZR ou segurar Y). Como o nome aparece só ali, **saiu do cabeçalho** da tela de cima (sobraram só os anunciadores; REC e HOLD Y passam para o canto esquerdo).
- **Novo nome: INTONARUMORI** (pasta no SD: `SD:/3ds/intonarumori/`; gravações em `.../rec/`).
- **6 abas**: HARSH · H-PRE · AMB · A-PRE · FILES · SET. PAUSE e REC viram ícones.
- **Ambient reescrito**: crossfade sem glide (acabou a "sirene"), voice leading, **pads de acorde** em tempo real com modificadores (7 / 9 / SUS / FLIP / INV), progressão MANUAL / SLOTS / prontas / GENERATE.
- **Lead**: DRIFT · ARP (com direção) · STEP SEQ · EUCLID · KEYS (+HOLD), eco ping-pong, **16 presets de ambiente** e dados (B sorteia acordes / notas).
- **Analógico** (circle pad + C-stick, ligável em SET: STICKS). **Y segurado 2–3 s = PAUSE**. SET ganhou STICKS, HOLD-Y e AUTO-MUT.
- **Arte em segmentos estilo Game&Watch** (fantasma / halo / núcleo): VOID refeita e 4 cenas de ambient — **mar, chuva, deserto, espaço**.
- **Anunciadores LCD** no cabeçalho: PL (tocando) · RC (gravando) · PA (pausa) · MU (mute) · BU (burst) · AM/HR (motor).

## Controles
- **Abas**: HSH · CDX-H · AMB · CDX-A · DRM · WAV (arquivos) · SET
- **L / R**: troca de página · **START**: sair · **SELECT**: alterna TOCAR ↔ EDITAR
- **Toque**: abas, pad XY, controles, **PAUSE**, **REC** (grava em `SD:/3ds/intonarumori/rec/REC_nnnn.wav`)
- **ZL / ZR**: play / pause (ou segurar Y 2–3 s)
- **HARSH**: ←→ demônio, ↑↓ volume, A mutate, B summon, X burst (segurar), Y mute (segurar). No editor: ↑↓ linha, ←→ valor
- **AMB**: ←→ acorde anterior/próximo, ↑↓ timbre da camada, A próximo acorde, B dados, X troca de painel (DRONE/LEAD), Y mute. No modo KEYS o pad toca o lead; em STEP SEQ toque nos 8 passos para mudar a nota. No editor: ↑↓ linha, ←→ valor
- **CDX-H / CDX-A**: toque no sigilo = carrega no harsh / no ambient · D-pad ←→ ±1, ↑↓ ±6
- **SET**: ↑↓ linha, ←→ valor
- **DRM**: A play/stop · B dados · ←→ kit
- **WAV** (arquivos): toque num arquivo para escolher · PLAY (ou B) ouve/para · RENAME (ou A) · DELETE (toque duas vezes)

⚠️ Harsh noise é intenso: comece com o volume baixo.

## Estrutura
`source/` código (synth, amb, rec, ui, art, gfx, main) · `Makefile` · `icon.png` · `host/` testes no PC (`make host`; `make intro` renderiza a abertura) · `.github/workflows/build.yml` build automático.

## Créditos
O motor gráfico anti-aliased (`gfx.c`, `gfx.h`, `mathx.h`) é portado do projeto **h1** (MIT) — licença em `source/LICENSE-h1.txt`.


## CODEX (1.0)
Aba CODEX: grade com os 30 sigilos. Toque = carrega o demonio no HARSH; toque de novo no mesmo = carrega no AMBIENT.

## FREEZE (1.0)
No AMB (tela de tocar), o botao FREEZE congela o reverb: o drone pode parar e a sala continua soando. Toque de novo para soltar. Ideia do "frozen room" do Penumbra (GPL-3.0).

## THROW (1.0)
No AMB, segure o botao THROW: o eco realimenta, o reverb aumenta e o brilho fecha; ao soltar volta suave. Inspirado nos "throws" do Penumbra (GPL-3.0).

## Paleta e JOURNEY (1.0)
Todo o app usa vermelho, preto e branco (visor VFD). Em SET, JOURNEY faz o ambiente viajar sozinho para o proximo preset (planeio de 12 s, tonalidade troca no meio), inspirado em journey.c do Penumbra (GPL-3.0).

## VFD e ALIVE (1.0)
Os paineis graficos simulam uma matriz de lampadas VFD/LED (desliga em LITE). Em SET, ALIVE (LOW/MID/HIGH) faz as vozes do drone entrarem e sairem sozinhas e a densidade do lead vaguear, inspirado no Penumbra (GPL-3.0).

## Referências portadas (GPL-3.0)
- **NOIZ DEMONS**: sigilos, paleta inicial, SUMMON.
- **Penumbra**: FREEZE, THROW, JOURNEY, ALIVE (ideias).
- **Kriechstrom**: DAMAGE (`krst_damage.c`), HOWL/MIC (`eng_glitch.c`), humores e padrões de bateria (`krst_dice.c`), nomes dos kits (`krst_kits.c`), swing/ratchet (`seq.c`). A síntese da bateria é própria e mais simples que a original.
