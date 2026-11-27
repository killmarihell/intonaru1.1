# HANDOFF — INTONARUMORI 1.1 (leia primeiro; o resto abaixo e historico da 1.0)
Mudancas da 1.1 (pedido do usuario, tudo testado so no PC com `make host`/`make intro`/`-fsyntax-only` do main.c; NADA no 3DS):
- Abas: `PG_HARSH, PG_CDXH, PG_AMB, PG_CDXA, PG_DRM, PG_FILES, PG_SET` (rotulos HSH CDX-H AMB CDX-A DRM WAV SET; `TAB_W`=36). Sumiram H-P/A-P (`H_PRESET`/`H_APRE` removidos); `H_CODEX` = harsh, `H_CODEXA` = ambient (ultimo do enum, usado pelo teste).
- Audio: `fill_buf` renderiza harsh (`g_hon`) + ambient (`g_aon`) + bateria (`gpat.on`) juntos; so renderiza o ligado (fade de 1 bloco ao ligar/desligar: `h_act`/`a_act`); soma harsh+amb com limitador suave. `g_engine` agora so escolhe o motor em foco na tela de cima. `H_HPLAY`/`H_APLAY` = botoes de liga/pausa.
- Bateria: `GrvBank` (4 padroes) em groove.h; `grv_render(g,p,bank,sel,chain,io,n)`; `grv_save/grv_load` (arquivo `IDRM` v1). main.c: `drum_sync/select/save/load`; carrega `drum.dat` no boot. Kit/bpm/swing/level sao globais; so celulas e tamanho (16/14) variam por padrao.
- UI: `play_btn`, `battery` (PTMU 0..5, `UiState.batt`), anunciadores H A D, `vfd_mask` ~31% (antes ~62%) tambem no codex/grade da bateria, `K_GREY` mais claro, abas ativas brancas. Titulo do preset limitado a 82 px em `art_display` (nao toca no HZ).
- Aba WAV toca as gravacoes: `Player` em rec.c/h (le o arquivo aos poucos no thread de escrita, anel de 16384 quadros; so WAV 16-bit 32728 Hz, cabecalho de 44 bytes). Mixado em `fill_buf` depois do `rec_push` (nao e regravado, sem DAMAGE) e tambem com o app pausado. `H_FPLAY` (ultimo do enum), tecla B na aba WAV.
- Pendente: ouvir no 3DS; CPU com 3 motores juntos no Old 3DS (se travar, desligar um); `ptmuInit` pode falhar (a bateria some, o resto segue).

# HANDOFF — INTONARUMORI 1.0 (em andamento)

> Para o agente que assume: leia este arquivo inteiro antes de mexer em qualquer coisa. Ele foi escrito para ser autossuficiente.
> O usuario fala portugues (Brasil). Responda em portugues.

## 1. O que e o projeto
INTONARUMORI e um instrumento de **harsh noise + ambient/drone** para **Old 3DS com Luma3DS** (homebrew `.3dsx`, C puro + libctru).
Tem dois motores de som, interface por toque + botoes, gravador WAV no SD e arte vetorial na tela de cima.
O usuario quer fazer a **versao 1.0 "ideal" de um grovebox noise/ambient**, absorvendo o maximo possivel de 3 repositorios de referencia.
A essencia do instrumento deve ser mantida (nao transformar em outro app).

## 2. Arquivos que voce recebe e o que fazer com eles
| Arquivo | Papel |
|---|---|
| `intonarumori_v1_0.zip` | **BASE DO TRABALHO.** Contem a pasta `intonarumori/` (codigo, Makefile, testes, README, este HANDOFF). |
| `noiz_demons-666.zip` | Referencia (so leitura). Ja usado: `firmware/src/goetia.c/.h` (30 demonios/sigilos) e a paleta dark. |
| `Penumbra-1.13.zip` | Referencia (so leitura). **Ainda NAO usado no codigo.** Ambient/space-drone. |
| `kriechstrom-0.5.2.zip` | Referencia (so leitura). **Ainda NAO usado no codigo.** Industrial/EBM, bateria, sequenciador, glitch. |

Os 3 repos sao **firmware do M-VAVE FM-1** (hardware diferente). **Nao copie codigo de hardware.** Leia a logica/DSP e **porte as ideias** para o motor em C puro do intonarumori (que roda igual no PC e no 3DS). Os 3 sao **GPL-3.0**; como `goetia.c/h` ja entraram, o projeto combinado e GPL-3.0 — creditar os repos no README (secao Creditos) quando algo for portado.

Passo 1: extraia cada zip em sua propria pasta nova e vazia. Trabalhe em copia da base.

## 3. Como validar (NAO ha devkitARM no ambiente de trabalho)
1. Dentro de `intonarumori/`: `make host` -> compila motor+UI+arte+gravador no PC com testes. **Tem que terminar com `RESULTADO: TUDO OK`.** (Gera `previews/*.ppm` — imagens das telas; converta com PIL para PNG e olhe com a ferramenta de imagem.)
2. `make intro` -> testa a abertura animada (ASAN/UBSAN).
3. Checar o `main.c` (que depende de `<3ds.h>`): crie um diretorio com um `3ds.h` que contem `#include "stub_3ds.h"` e rode  
   `gcc -fsyntax-only -I<dir_do_3ds.h> -Ihost -Isource source/main.c` (deve sair sem erro).
4. O `.3dsx` real so e gerado no **GitHub Actions** (`.github/workflows/build.yml`, artefato `INTONARUMORI_1.0_para_cartao_SD`). **Nada disto foi testado no aparelho real.** Diga isso ao usuario e peca para ele compilar/testar.
5. Ao adicionar controle novo/tipo de toque: atualizar os limites de `H_*` em `host/test_host.c` (o teste "mapa de toque" usa o maior valor do enum, hoje `H_AVOL`).

## 4. Mapa do codigo (`source/`)
- `synth.c/.h` — motor HARSH (parametros `P_*` float 0..1, 16 presets, `patch_randomize/mutate/summon`).
- `amb.c/.h` — motor AMBIENT: drone (acordes, crossfade sem glide, voice leading) + lead (DRIFT/ARP/SEQ/EUCLID/KEYS), eco ping-pong, reverb de combs, 16 presets (`amb_preset`), dados (`amb_dice_*`). API detalhada na secao 9.
- `ui.c/.h` — TODA a interface (desenha direto no framebuffer RGB565) e o mapa de toque (`ui_hit`). Paleta `K_*` no topo. `UiState` e o contrato com o `main.c`.
- `art.c/.h` — arte vetorial anti-aliased (cenas antigas, abertura `art_intro`, `art_display` = painel direito com osciloscopio/Hz). `acc[]/acc2[]` = cores de acento.
- `goetia.c/.h` — 30 demonios (sigilos como segmentos de linha) + profundidade por demonio. **Origem: noiz_demons (GPL-3.0).**
- `gfx.c/.h`, `mathx.h` — motor grafico (do projeto h1, MIT, `LICENSE-h1.txt`).
- `rec.c/.h` — gravador WAV.
- `main.c` — loop principal, audio (NDSP), entrada, estado (`page`, `sub`, `preset`, `ademon`, `demon`...).
- `host/` — testes no PC (`test_host.c`, `test_intro.c`, `stub_3ds.h`). `Makefile`: `make` (3DS), `make host`, `make intro`.

## 5. O que JA foi feito na 1.0 (e como esta implementado)
1. **Slider MASTER do AMB** no rodape da tela de tocar (DRONE e LEAD): `ui.c` `draw_avol`, `r_avol`, `UI_AVOL_Y`, hit `H_AVOL`; `main.c` trata toque e arrasto (`ap.p[A_VOL]`). Substituiu as linhas de dica do rodape do AMB (as dicas de botoes ficam so no README).
2. **Volume**: `amb.c` — `gtarget = A_VOL*1.6` (era 0.9) e `dg = A_DVOL*1.15` (era 0.62/1.5). Drone ~+9 dB, master ~+4 dB. Presets de ambient: RMS ~0.2–0.3, pico < 0.8 (medido em `make host`). **Medido so por numeros — o usuario deve confirmar de ouvido.** O slider de volume do HARSH foi preservado (o usuario gosta dele).
3. **Visual dark (noiz demons)**: paleta `K_*` em `ui.c` (fundo quase preto esverdeado, verde fosforo `RGB(108,255,58)`, vermelho ritual `RGB(236,26,34)`); `art.c` `acc/acc2` todos fosforo/vermelho; abertura recolorida.
4. **SUMMON (harsh)**: botao RANDOM virou SUMMON; B tambem. Sorteia 1 dos 30 demonios; `patch_summon(v, depth)` (`synth.c`) reroll de cada parametro com prob. `depth/255`; `goetia_demon_depth()` da a profundidade (Furcas ~15% ... Belial 100%).
5. **Sigilos na tela de cima** (substituem as cenas antigas no quadro esquerdo): `ui.c` `draw_demon` + `scene_demon()` — halo + nucleo, pulsa com o nivel, treme acima de 0.6. Qual demonio: SUMMON/navegacao > preset (harsh `(2i)%30`, ambient `(2i+1)%30`).
6. **Presets renomeados** com nomes dos demonios (harsh: `preset_names[]`; ambient: `PRE[]` em `amb.c`). Nomes antigos: harsh DRONE STATIC HARSH SHREDDER DIGITAL FEEDBACK INDUSTRIAL VOID WALL GLITCH GRAIN TURBINE SCREECH EMBERS STUTTER SUBQUAKE; ambient TIDE HARBOR NEON-NIGHT LIGHTHOUSE WINDOW-RAIN COLD-FRONT CATHEDRAL LULLABY DUNE MIRAGE DAWN HYMN DEEP-SPACE ORBIT GLACIER CRYSTALS. O mapeamento e **so por indice** (o demonio nao combina de proposito com o som — ver pendencia 7.8).
7. **30 demonios navegaveis**:
   - HARSH: setas ◄ ► na tela e D-pad ←→ percorrem 30 (`demon_load` em `main.c`): k par = preset k/2; k impar = preset k%16 rerolado por `patch_summon` com semente fixa `k*7919+13` (som estavel).
   - AMBIENT (aba A-PRE): D-pad ←→ (±1) e ↑↓ (±5); k par = preset k/2; impar = preset k%16 + `amb_dice_chords/lead` com sementes fixas. `ademon`/`UiState.ademon`; tocar num tile da grade zera `ademon`.
8. **Versao 1.0** em README, build.yml, Makefile, abertura ("V1.0") e rodape da pagina SET.

## 6. Estado honesto
- Usado de verdade nos repos de referencia: **so noiz_demons** (sigilos, paleta, SUMMON).
- **Penumbra e kriechstrom: so leitura de README.** Nada deles esta no codigo ainda. O usuario quer que sejam **bem aproveitados** — e a prioridade das proximas etapas.
- Aba AMB: paleta dark ok, mas o layout e os controles sao os da 0.9 repintados; o lead ainda usa amarelo/ambar (destoa do verde/vermelho); o som do ambient e o motor da 0.9 (os 30 demonios do ambient so trocam preset/acordes/melodia).
- A grade de presets (H-PRE/A-PRE) ainda tem 16 tiles; os 30 so sao acessiveis por navegacao com D-pad/setas.

## 6b. FEITO NA SESSAO 2 — Aba CODEX (pendencia 1 concluida)
- Nova pagina `PG_CODEX` (7a aba, entre A-PRE e FILES; `TAB_W` caiu para 34). Grade 6x5 com os 30 sigilos (`page_codex`, `r_cx` em ui.c). Hit `H_CODEX` (arg 0..29; e o ultimo do enum -> teste usa `H_CODEX` como maximo).
- 1o toque no sigilo = carrega o demonio no HARSH (`demon_load`); tocar de novo no MESMO = carrega no AMBIENT (mesma regra de A-PRE: par = preset k/2; impar = preset k%16 + dice com semente fixa). Estado local `csel` em main.c -> `UiState.codex_sel`.
- Abas agora: HARSH H-PRE AMB A-PRE CODEX FILES SET (SET = indice 6). Testes atualizados. `make host` TUDO OK, `make intro` OK, `-fsyntax-only` do main.c OK. Nao testado no 3DS.
- PROXIMA: pendencia 2 (FREEZE do reverb).

## 6c. FEITO — FREEZE do reverb (pendencia 2 concluida)
- `AmbIn.freeze` (campo novo no FIM do struct) + `Amb.frz` (suavizado). Em `amb_render`: entrada dos combs `in_m *= (1-frz)`, feedback -> 0.996, wet sobe a ate 0.9. Sem repetir o drone: a sala congelada segue soando.
- Gatilho: botao FREEZE/FROZEN na tela AMB de tocar (rodape, a esquerda do slider MASTER, que perdeu o rotulo de texto). `H_FREEZE` (agora o ultimo do enum), `UiState.freeze`, `g_freeze` em main.c. Toggle por toque (nao e toque longo).
- Teste novo "FREEZE" em test_host.c (cauda com freeze > 2x sem). `make host` TUDO OK, syntax main.c OK. Nao testado no 3DS; confirmar de ouvido.
- PROXIMA: pendencia 3 (throws/gestos, Penumbra fxbus.c).

## 6d. FEITO — Throws (pendencia 3 concluida) + ajuste do CODEX
- CODEX: nomes na grade cortados em 7 letras (e no hifen: GLASYA-LABOLAS -> GLASYA); sigilo e texto subidos para nao encostar na borda.
- THROW: botao no rodape do AMB (direita do slider MASTER, `r_thr`, `H_THROW` = ultimo do enum, `UiState.throwing`, `g_throw`). Enquanto SEGURA: `AmbIn.thr` -> `Amb.thr` (sobe ~0,15 s, volta ~1 s): feedback do eco 0.48->0.92, mix do eco +0.6, wet do reverb +0.5, brilho fecha (lpa/lpb -65%). Um unico throw combinado (shimmer real nao foi portado).
- Teste "THROW" (rms sobe, pico < 0.98). `make host` TUDO OK. Nao testado no 3DS.
- PROXIMA: pendencia 4 (viagem de cenas, Penumbra journey.c; SET_N sobe de 6).

## 6e. FEITO — Paleta vermelho/preto/branco + JOURNEY (pendencia 4 concluida)
- PALETA (pedido do usuario): `K_*` em ui.c e macros `CG/CL/CY/CA/CW/CR`, `BG_ART`, mesh em art.c agora sao preto (6,3,3), vermelho VFD (255,48,48), branco (244,238,236), rosa claro e vermelho-laranja. Mantidos: animacoes, segmentos VFD apagados (fantasma vermelho escuro) x acesos. Nomes `K_GREEN/K_YELL` etc. sao so apelidos: K_GREEN = vermelho principal, K_YELL = branco (aba AMB e lead em branco). Nao reintroduzir verde/ambar.
- JOURNEY: `amb_travel(out,a,b,t)` em amb.c/h (lerp dos continuos; discretos, acordes, seq, scene, mask trocam em t>=0.5; A_VOL intacto). main.c: bloco JOURNEY antes do AUTO-MUT: a cada 30/60/120 s (SET -> JOURNEY, 7a linha; `SET_N`=7, `SET_DY`=24) vai para `(apreset+1)%16` planando 12 s (720 quadros a 60 fps), troca preset/acorde no meio. Mexer em preset/demonio cancela. So roda com AMB ativo e sem pausa. Rodape do SET compactado.
- Testes: JOURNEY (amb_travel) + coordenadas do SET atualizadas. `make host` TUDO OK. Nao testado no 3DS.
- PROXIMA: pendencia 5 (ALIVE, vozes que cantam sozinhas, Penumbra vox.c).

## 6f. FEITO — Look VFD/LED + ALIVE (pendencia 5 concluida)
- PEDIDO DO USUARIO: "todas as luzes sao VFD, o app todo simula um LCD/LED, principalmente os paineis graficos". Feito: `vfd_mask()` em ui.c (grade de pontos: pixel em x%3==2 ou y%3==2 escurece ~62%) aplicada nos 2 paineis graficos da tela de cima (sigilo e osciloscopio/Hz) e no pad XY do HARSH; desligada com LITE. Ja existiam segmentos apagados fantasma (mulc 34) nos medidores. IDEIAS DE CONTINUACAO: aplicar tambem no pad do lead/EUCLID do AMB e no CODEX; brilho/bloom nos acesos; cantos dos paineis.
- ALIVE: SET -> ALIVE (8a linha; `SET_N`=8, `SET_DY`=22, rodape do SET so 2 linhas + versao). `AmbIn.alive` (0..3); `Amb.al_g[4]/al_t/al_d/al_dt/al_clk`. Em `amb_render`: a cada 2-7 s uma das 4 "familias" de voz do drone (voz i -> i&3) entra/sai (ganho 1 <-> 1-amt, sempre sobram 2), densidade do lead vagueia ate +-0.5*amt. Teste "ALIVE" (OFF constante; HIGH varia; pico < 0.98).
- `make host` TUDO OK. Nao testado no 3DS.
- PROXIMA: pendencia 6 (DAMAGE no master + HOWL/MIC no harsh, Kriechstrom krst_damage.c / eng_glitch.c).

## 6g. FEITO (sessao 3) — DAMAGE no master + HOWL/MIC (pendencia 6 concluida)
- `source/damage.c/.h` (novo): porte em float de Kriechstrom `krst_damage.c` (FITA <-> DIGITAL, quedas, guarda de nivel que so abaixa). Aplicado no `fill_buf` ao mix final dos dois motores. 0% = bit a bit igual.
- HOWL: `P_HOWL`, `P_HMODE` (STRING/MIC) no modulo NOISE do editor harsh. Os sorteios (SUMMON/MUTATE/random) PULAM esses dois, para os demonios com semente fixa soarem iguais.
- SET tem 10 linhas (`SET_DY`=19): DAMAGE e DMG CHAR sao sliders (set_n == 0).
- Testes HOWL e DAMAGE em test_host.c. Makefile (host/intro) lista damage.c. So testado no PC.
- PROXIMA: pendencia 7 (DICE por humores).

## 6h. FEITO (sessao 3) — DICE por humores (7) + GROOVE (8)
- `source/groove.c/.h` (novo): 4 vozes (KICK SNR HAT METL), 7 kits (nomes do Kriechstrom, sintese propria em float), 16 passos (MARCH = 14), ratchet x1..x4, swing, 8 humores com os padroes de `krst_dice.c`. Misturado no `fill_buf` ANTES do DAMAGE.
- `amb_dice_mood()` em amb.c: com humor escolhido, DICE/B do ambient segue o humor (`adice()` em main.c).
- Aba nova `PG_GRV` (8 abas, `TAB_W`=30, rotulos HSH H-P AMB A-P CDX GRV WAV SET). Hits `H_GPLAY..H_GSLIDE` (`H_GSLIDE` e o ultimo do enum).
- Corrigido: botao A na aba de arquivos nao muta mais o patch; texto da escala no AMB nao encosta no FREEZE.
- Conferido: 30 demonios harsh + 16 presets ambient geram audio identico ao da base (HOWL/DAMAGE em 0); ASAN/UBSAN limpo.
- NADA testado no 3DS: ouvir kits/volume, DAMAGE, HOWL e ver se o Old 3DS aguenta a CPU com tudo ligado.
- PROXIMA: 9 (ambient com identidade nos 30), 10 (demonio x intensidade), 11 (visual da aba AMB). O padrao do GROOVE nao e salvo no SD (ideia futura).

## 6i. FEITO (sessao 3, parte 2) — pendencias 9, 10, 11 (parcial)
- 9: `amb_load_demon()` / `amb_preset_demon()` em amb.c. Corrigido: demonio k agora carrega o preset com o NOME dele (antes carregava o de outro nome); os 14 sem preset geram ambiente proprio pela profundidade. Teste "30 demonios no ambient".
- 10: `preset_sound()` em synth.c permuta os 16 sons por intensidade (medida: rms x brilho) conforme a profundidade; `preset_demon()` / `demon_preset()`. Slot 15 = ASMODEUS (era FURCAS repetido). App abre no slot 15 (o mesmo som de antes). Os 16 sons sao identicos, so mudaram de lugar; os demonios gerados (harsh) soam diferentes dos da sessao anterior.
- 11: interferencia no sigilo ao trocar de demonio; titulo da tela de cima mostra o demonio. NAO feito: sigilo grande de fundo na tela de baixo (as telas estao cheias de controles).
- RESTA: ajuste por ouvido no 3DS (ordem de intensidade, 14 ambientes novos, volume da bateria, CPU). Ideias: salvar padrao do GROOVE / patches no SD.

## 6j. Conferencia final (pendencia 12): controles do README revisados contra o main.c; versao 1.0. Falta so o teste no 3DS.

## 7. PENDENCIAS (1 a 10 feitas; 11 parcial; 12 docs feitos) (ordem recomendada — cada uma e um passo pequeno, valide com `make host` antes de seguir)
1. **Aba CODEX**: nova pagina com os 30 sigilos em grade (ex.: 6x5, cada celula com `goetia_draw` em escala pequena, nome embaixo). Tocar = carrega o demonio no harsh; (tocar de novo/segurar) = carrega no ambient. Mexer em: `PG_COUNT` e enum de paginas (`ui.h`), `r_tab`/`tabs()` e `ui_hit` (`ui.c`; hoje 6 abas HARSH, H-PRE, AMB, A-PRE, FILES, SET — pode ser necessario encurtar rotulos ou substituir H-PRE/A-PRE por CODEX), `main.c` (dispatch de pagina), teste em `host/test_host.c`.
2. **FREEZE do reverb** (Penumbra `dsp/spacevb.c`): `AmbIn.freeze` -> em `amb_render` zerar a entrada dos combs (`in_m`) e `fb≈0.995`. Falta escolher o gatilho: todos os botoes fisicos ja tem uso; sugestao = **toque longo num icone na tela do AMB**.
3. **Throws/gestos** (Penumbra `dsp/fxbus.c`): enquanto segura um controle, burst de shimmer/feedback/dip do mix; ao soltar volta suave.
4. **Viagem de cenas** (Penumbra `seq/journey.c`): a cada N compassos planar (lerp) de um `amb_preset` para o proximo (raiz/escala/mix + crossfade de acordes). Opcao nova em SET (`SET_N` hoje 6: SMOOTH, SPRING, LITE, STICKS, HOLD-Y, AUTO-MUT; ao subir, ajustar `r_srow`, `set_n[]`, `setv[]` em main.c e o teste "SET").
5. **Vozes que cantam sozinhas / ALIVE** (Penumbra `dsp/vox.c`, `app/penumbra.h`): densidade do lead e entrada/saida de vozes do drone em aleatorio lento.
6. **DAMAGE no master + HOWL/MIC no harsh** (Kriechstrom `krst_damage.c`, `eng_glitch.c`).
7. **DICE por humores** (Kriechstrom `krst_dice.c`, 8 moods) nos botoes DICE do AMB.
8. **GROOVE**: kits de bateria de ruido + sequenciador com swing/ratchet (Kriechstrom `krst_kits.c`, `drum_voice.c`, `seq.c`, `tests/swing_test.c`, `ratchet_test.c`).
9. **Completar os 30 do ambient com identidade propria**: hoje os impares sao preset+dados; idealmente cada demonio com ambiente proprio.
10. **Combinar demonio com som**: reordenar presets de harsh por intensidade para o demonio (ordem do registro: calmo -> caotico) combinar com o som.
11. **Mais dark** e coerencia da aba AMB: lead em verde/vermelho, bordas vermelhas em botoes ativos, sigilo grande como fundo da tela de baixo, flicker/glitch ao trocar de demonio.
12. Ao fim: README (novidades 1.0 completas, controles, creditos Penumbra/Kriechstrom), conferir versao em todos os lugares.

## 8. Regras de trabalho com este usuario (importante)
- O usuario trabalha com **limite curto de tempo por sessao (~8 min)** e troca de agente quando acaba. **Entregue em passos pequenos e seguros**, rodando `make host` a cada passo, e **sempre reenvie o zip atualizado** (`intonarumori_v1_0.zip`, sem `build/` e `previews/`) + **atualize este HANDOFF** antes de acabar o tempo.
- Ele pediu "faca as coisas mais faceis, tenho medo de voce tentar e acabar": prefira o passo menor que fecha limpo a um grande pela metade. Nunca deixe o projeto sem compilar.
- Seja honesto sobre o que foi testado: so PC (`make host`); **nada no 3DS real**.
- Ele quer **mais abas, mais opcoes e mais funcoes** vindas dos repos de referencia; "o maximo dessa versao".

## 9. API do motor ambient (de `amb.h`)
`AmbPatch`: `p[A_COUNT]` 0..1, `mask`, `seq[8]`, `prog[8]`, `drone_on`, `lead_on`, `cdeg`, `cmod` (CM_7/9/SUS/FLIP/INV), `cgen` (++ a cada toque num pad reaciona o acorde), `scene`.
Grupos do editor: DRONE(7) LEAD(6) PLAY(7) MIX(7). Modos do lead: `L_DRIFT, L_ARP, L_SEQ, L_EUCL, L_KEYS, L_OFF(=5, so UI)`. Estado para a UI: `amb.cur_deg_ui`, `step_ui`, `nsteps_ui`, `hit_ui`, `venv[]`.
Helpers: `amb_chord_label`, `amb_roman`, `amb_nd`, `amb_plen`, `amb_set_slot`, `amb_write_chord`, `amb_dice_chords(a, seed)`, `amb_dice_lead(a, seed)`, `amb_preset(a, n)`, `amb_preset_name`, `amb_preset_scene`, `amb_render`. `AmbIn { touch, x, y, mute, cx, cy }`.
Layout: tela de baixo 320x240; editor `ROW0=70`, `ROW_H=22` (max 7 linhas por grupo). Dica de armadilha: `ui_hit` e o desenho em `ui.c` devem concordar; o teste "mapa de toque" varre todos os pixels e paginas.

## 10. Armadilhas ja encontradas
- `UiState` e inicializada com designated initializers em `main.c` e nos testes: campo novo fica 0 nos testes (ex.: `ademon` = 0 -> mostra demonio 0 no AMB nos previews).
- Editar `ui.c`/`main.c` com substituicao de texto: conferir que o trecho existe (ja houve `assert` falhando e chaves `}` perdidas num `case`). Sempre rodar o `-fsyntax-only` do `main.c`.
- Ao trocar o `A_COUNT` ou a ordem de parametros do ambient, os presets `PRE[]` (posicional) quebram.
- Fixar sementes (`patch_seed`) afeta o RNG global do harsh: depois restaure com `patch_seed(rand()|1)`.

## PLANO DE APROVEITAMENTO — PENUMBRA E KRIECHSTROM (LER ANTES DE CODAR)
Honestidade: ate a fase 5 SO o noiz_demons foi usado no codigo. Penumbra e kriechstrom foram lidos so pelos READMEs. O proximo agente deve LER OS ARQUIVOS abaixo e portar a logica (nao o codigo FM-1/hardware) para o motor C puro do intonarumori. Ambos sao GPL-3.0 (o projeto ja vai ser GPL-3.0). Creditar em README + CREDITS.

### Penumbra (zip: Penumbra-1.13; caminhos em firmware/src/)
- `seq/journey.c/.h`  -> VIAGEM entre cenas (planar voz a voz, 12 s, tonalidade muda junto, 7 jornadas prontas). Portar como: lista de cenas = presets de ambient; lerp de AmbPatch.p[] + crossfade de acordes. Ver tambem `app/penumbra.h` (parametros ALIVE).
- `dsp/drone.c`       -> drone de varias vozes (FILTER/BODY/SWARM/WAVE, vozes que entram e saem). Comparar com amb.c `wtab`/vozes; trazer SWARM (detune em enxame) e vozes HELD vs que vem e vao.
- `dsp/spacevb.c`     -> reverb "space" + FREEZE (sala congelada) + shimmer. Substitui/complementa o reverb de combs em amb.c.
- `dsp/vox.c`         -> a voz que canta sozinha (HALO). Base para "vozes que cantam" no lead (ALIVE).
- `dsp/texture.c`, `dsp/pulse.c` -> textura/pulso de fundo; ver se rende uma camada TEXTURE (ruido filtrado lento) no ambient.
- `dsp/fxbus.c`       -> "throws" (FX held: shimmer burst, freeze, dip do mix que volta ao soltar).
- `seq/looper.c`      -> loop que segue a mudanca de tonalidade (loops guardados em graus da escala).
- Doc: MANUAL.md e README.md (secao "More it can do").

### Kriechstrom (zip: kriechstrom-0.5.2; caminhos em firmware/src/)
- `krst_dice.c`   -> DICE com 8 MOODS (MACHINE, GRIND, MARCH 7/8, GLITCH, SPIRAL, GHOST, SURGE, HUSH). Portar para amb_dice_* / harsh: cada mood = regras de padrao. Bom para o botao DICE do AMB e para um SEQ de bateria.
- `krst_kits.c` + `drum_voice.c` -> kits de bateria proprios (ROOM, GATE, MARCH, ARC, ROT, SLAB, DESTROY): base para a camada GROOVE (kick/snare/hat de ruido) no harsh.
- `seq.c` (+ swing, ratchet em tests/swing_test.c, ratchet_test.c) -> sequenciador com swing e ratchet.
- `krst_damage.c` -> DAMAGE no master (fita <-> digital). Candidato direto a um knob/aba no harsh e no master do ambient.
- `krst_haunt.c`, `krst_infect.c`, `krst_resonate.c`, `krst_splice.c`, `krst_speak.c` -> efeitos "assombrados" (ler cada cabecalho: comentario no topo explica a ideia). Muito alinhados com o tema demoniaco.
- `eng_glitch.c`  -> GLCH: clip, wavefold, filtro ressonante, bitcrush, micro-stutter; HOWL/MIC (loop que se alimenta, grita afinado) = otimo para o harsh.
- Doc: README.md ("What it adds to Felucca").

### Ordem sugerida (valor/risco)
1. Aba CODEX (30 sigilos) — ja descrito.
2. FREEZE + throws (spacevb/fxbus) — pequeno, muda muito o drone.
3. Viagem de cenas (journey.c) + ALIVE (vox.c).
4. DAMAGE no master + HOWL/MIC no harsh (krst_damage.c, eng_glitch.c).
5. DICE por moods (krst_dice.c) no AMB.
6. GROOVE: kits + seq com swing/ratchet (krst_kits.c, drum_voice.c, seq.c).
Cada passo: make host TUDO OK + novo teste em host/test_host.c; subir versao so no fim.


---
# ARQUIVO: handoff da 0.9 (historico)
# HANDOFF — INTONARUMORI 1.0 (EM ANDAMENTO)

Pasta: /home/claude/ex/92f9999d-intonarumori_v0_9/intonarumori. Reps de referência extraídos em /home/claude/ex/{Penumbra-1.13,noiz_demons-666,kriechstrom-0.5.2} (só leitura; são firmware FM-1 em C).
Validação: `make host` (TUDO OK) + `gcc -fsyntax-only -I<dir com 3ds.h que inclui host/stub_3ds.h> -Isource source/main.c`. Sem devkitARM aqui.

## Feito na 1.0 (pedidos do usuário)
- [x] Slider MASTER do AMB (ui.c `draw_avol`, `r_avol`, H_AVOL; main.c arrasto). Substituiu as 2 linhas de dica no rodapé do AMB.
- [x] Volume: amb.c `gtarget` A_VOL*1.6, `dg` = DVOL*1.15 (antes 0.9 e 0.62/1.5). Presets: rms 0.2–0.3, pico <0.8.
- [x] Visual dark NOIZ DEMONS: paleta K_* em ui.c (preto-esverdeado/fósforo/vermelho). art.c ainda usa cores de acento próprias por cena (revisar para combinar).
- [x] SUMMON: goetia.c/h copiados do noiz_demons (GPL-3.0); `patch_summon(v, depth)` em synth.c; `goetia_demon_depth()` dá a profundidade; sigilo desenhado em ui.c `draw_demon` (UiState.demon/demon_t, 90 quadros). Botão RANDOM virou SUMMON; B no harsh também.
- [x] SIGILOS NAS TELAS: ui.c `draw_demon` substituiu art_scene no quadro esquerdo da tela de cima (halo+nucleo, pulsa com nivel, tremor >0.6). `scene_demon()`: SUMMON > preset*2 (harsh) / apreset*2+1 (amb). art.c: paleta toda fosforo/vermelho. art_scene ainda existe (ui_vis_test/abertura).
- [x] Versão 1.0 em README, build.yml, Makefile, abertura ("V1.0"), rodapé SET.

## PENDENTE (usuário pediu "mais funções, mais abas, mais opções" absorvendo os reps)
Ideias já avaliadas, em ordem de valor:
1. **FREEZE do reverb** (Penumbra "frozen room"): `AmbIn.freeze` → em amb_render zerar `in_m` e fb≈0.995. Falta escolher botão (D-pad/ABXY/L/R/ZL/ZR estão todos usados; sugestão: toque longo num ícone na tela AMB).
2. **Viagem de cenas** (Penumbra "journey"/TOTALITY): a cada N compassos, planar (lerp) para o próximo amb_preset(n) (raiz/escala/mix). Opção nova em SET (SET_N sobe de 6 e ajustar r_srow/set_n/teste).
3. **Vozes que cantam sozinhas / ALIVE** (Penumbra): densidade do lead e entrada/saída de vozes do drone em aleatório lento.
4. **Aba SCENES/DEMONS**: codex com os 30 sigilos (grade) e escolha manual do demônio (profundidade fixa); SUMMON também no AMB (profundidade = quanto dice/mutate muda acordes+timbre+mix).
5. **Gestos/throws** (Penumbra FX held): burst de shimmer, dip do mix, voltando ao soltar.
6. **Sequenciador/Groovebox** (Kriechstrom): trilha de bateria noise (kick/hat/snare por ruído filtrado) com swing e ratchet; pad de slicer sobre gravações do REC.
7. Amb: sidechain/pump do drone no kick.
Ao adicionar aba: PG_COUNT, tabs(), r_tab, ui_hit, teste test_host (limites de H_*).

## Fase 3 (nomes)
- Presets renomeados para os demonios das telas: harsh i -> registry[(2i)%30]; ambient i -> registry[(2i+1)%30] (i=15 -> 28). Nomes antigos: harsh DRONE STATIC HARSH SHREDDER DIGITAL FEEDBACK INDUSTRIAL VOID WALL GLITCH GRAIN TURBINE SCREECH EMBERS STUTTER SUBQUAKE; ambient TIDE HARBOR NEON NIGHT LIGHTHOUSE WINDOW RAIN COLD FRONT CATHEDRAL LULLABY DUNE MIRAGE DAWN HYMN DEEP SPACE ORBIT GLACIER CRYSTALS. Ordem do registry = do mais calmo (Furcas) ao mais caotico (Belial): reordenar presets harsh por intensidade para o demonio combinar com o som (hoje o mapeamento e so indice).
- Ainda mais dark: reduzir brilho dos K_GREY/K_GRID, bordas vermelhas nos botoes ativos, sigilo grande como fundo da tela de baixo, glitch/flicker no troca de demonio.
- Ideias nao feitas: ver lista da fase 1 (freeze, jornada de cenas, vozes que cantam, aba codex, throws, groovebox).

## Fase 4 (30 demonios)
- Harsh: ◄ ► (tela) e D-pad ←→ percorrem os 30 demonios (main.c `demon_load`): par k = preset k/2; impar k = preset k%16 rerolado por `patch_summon` com semente fixa (k*7919+13) => som estavel. Preset 15 so pela grade. A grade H-PRE continua com os 16 presets.
- NAO feito: grade/aba de 30 (codex) na H-PRE/A-PRE; ambient ainda so 16 (sigilos 2i+1 repetem/colidem) -> gerar 14 ambientes por demonio com amb_preset(k%16)+amb_dice_*; nome do demonio no titulo da tela de cima mostra o preset antigo? (conferir `d.title`).
- Proximo passo recomendado: aba CODEX (PG_CODEX, grade 6x5 com sigilo pequeno desenhado via goetia_draw, toque = carrega demonio no harsh; longo = no ambient), depois freeze/jornada/vozes.

## Fase 5 (ambient com 30)
- Aba A-PRE: D-pad ←→ (±1) e ↑↓ (±5) percorrem os 30 demonios (main.c, `ademon`; UiState.ademon). Par k = preset k/2; impar k = preset k%16 + `amb_dice_chords/lead` com semente fixa (k*7919 / k*104729) => sempre igual. Tocar num tile da grade zera `ademon` (volta ao demonio do preset). O sigilo no AMB segue `ademon`.
- A grade A-PRE continua com 16 tiles; falta a aba CODEX (30 sigilos clicaveis). Nao testado no 3DS.

