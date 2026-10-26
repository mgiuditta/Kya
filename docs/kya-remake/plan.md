# Kya Remake (UE5): piano dalla vertical slice

Piano costruito sulle skill di `awesome-gamedev-agent-skills`. Per ogni sezione è indicata la skill da cui viene.
Target: Unreal Engine 5.8, progetto solista e non commerciale, asset rifatti da zero; gli originali servono solo come riferimento.

---

## 0. Principio guida (`prototype-fast`, `prototype`)

Il prototipo serve a prendere una **decisione** (tenere, buttare o rifare) e non è un prodotto. Ogni prototipo risponde a **una sola domanda**.
La vertical slice ha tre pilastri, quindi si parte da **tre spike separati** e la slice si fa dopo:

| Spike | Domanda unica | Timebox | Tieni se | Butta se |
|---|---|---|---|---|
| A. Vento | "Cadere e planare nelle correnti è piacevole da controllare?" | 2-3 sere | un giocatore nuovo cerca le correnti da solo entro 30 s e rifà il salto "un'altra volta" | funziona solo dopo che glielo spieghi |
| B. Scivolata | "Scivolare in un canyon dà velocità e controllo insieme?" | 2-3 sere | si sente la velocità senza perdere la linea | è solo un rettilineo in cui tieni premuto avanti |
| C. Combattimento + liberazione | "Battere un Wolfen e liberarlo è un momento soddisfacente?" | 3-4 sere | il colpo "pesa" e la liberazione si fa ricordare | è un "abbassa la barra della vita e premi un tasto" |

Regole:
- Tutto in greybox: capsule, cubi, Modeling Mode (Cube Grid). Niente arte finché il verbo non convince.
- Ogni spike sta in una cartella `Content/Prototypes/<nome>` e su un branch usa e getta. Un esito "tieni" vuol dire **riscrivere** il codice nel progetto vero, non copiarlo.
- Una HUD di debug a schermo con i numeri che si stanno valutando: velocità, tempo in aria, forza del vento, finestre di input.

---

## 1. Architettura tecnica (`unreal-cpp-gameplay`, `unreal-blueprints`)

**C++ per lo scheletro, Blueprint per il tuning e il contenuto.**

- `AKyaCharacter : ACharacter` in C++. Il `UCharacterMovementComponent` riceve **movement mode custom** (`MOVE_Custom`):
  - `KyaMove_Slide`: scivolata
  - `KyaMove_WindGlide`: caduta o planata nel vento
  Le transizioni tra corsa, scivolata, caduta e vento si gestiscono nel movement component (`PhysCustom`, `OnMovementModeChanged`), non nell'Event Graph. È questo che rende la transizione "continua" chiesta nella scheda.
- Tutti i parametri di feel sono `UPROPERTY(EditAnywhere)`, così si regolano in un Blueprint figlio (`BP_Kya`) senza ricompilare.
- `AKyaGameMode` imposta `DefaultPawnClass` e `PlayerControllerClass`.
- Comunicazione tra Blueprint:
  - **Interface** `BPI_Liberable` sui Wolfen: la chiama Kya e non serve nessun Cast.
  - **Event Dispatcher** `OnWolfenFreed` per broadcast verso HUD, città dei Nativs e audio.
  - Niente logica riutilizzabile nel Level Blueprint e niente `Event Tick` dove basta un evento o un timer.
- Moduli in `Build.cs`: `EnhancedInput`, `Niagara`, `AIModule`, `GameplayTasks`, `NavigationSystem`.
- Combattimento: per la slice basta un `UKyaCombatComponent` semplice (combo come array di montage più finestre di cancel). **GAS (Gameplay Ability System) è rimandato:** serve solo quando ci saranno abilità, bracciali e potenziamenti numerosi.

---

## 2. Pilastro A: Vento e caduta libera (`platformer`, `physics-tuning`, `unreal-niagara`)

- Le correnti sono volumi `AWindVolume` con direzione, forza e una curva di falloff. Mentre `KyaMove_WindGlide` è attivo, il character somma le forze dei volumi che sta attraversando.
- I numeri si derivano dal feel e non si tirano a caso (Pattern 1 di `platformer`):
  - salto: altezza e tempo all'apice → gravità e velocità iniziale
  - gravità in caduta 1.5-2.0× quella in salita, così niente "floaty"
  - **velocità terminale** diversa tra caduta libera e planata: è il knob principale del vento
  - forte controllo in aria durante la planata, poco in caduta normale
- Feel aid da avere subito: coyote time di circa 0.1 s, jump buffer di circa 0.12 s, salto variabile (velocità ×0.45 al rilascio).
- Tutto gira sul tick di fisica fisso, con interpolazione del render. Per l'alta velocità serve CCD sulla capsula, altrimenti si attraversano le piattaforme sottili (tunneling).
- VFX: un sistema Niagara `NS_WindCurrent` sul volume, con parametri **User** (`User.Strength`, `User.Direction`) impostati dal volume stesso. Le emitter GPU hanno bisogno di bounds fissi. Le correnti devono essere **leggibili**: il giocatore deve capire da lontano dove porta il vento.

## 3. Pilastro B: Scivolata (`platformer`, `physics-tuning`, `game-feel`)

- `KyaMove_Slide` si attiva su superfici marcate (Physical Material `PM_Slide`) o con un input.
- Knob: accelerazione lungo la pendenza, attrito, sterzata (bassa ad alta velocità), salto dalla scivolata che conserva il momento.
- La sensazione di velocità viene da FOV dinamico, camera più bassa, motion lines, audio del vento e scie sulle superfici. Non deve venire solo dal numero di velocità.

## 4. Pilastro C: Combattimento e liberazione dei Wolfen (`game-feel`, `unreal-behavior-trees`, `ai-behavior-trees-utility-ai`, `game-ai`)

**Feel del colpo:** ogni colpo a segno attiva un bundle di 5-8 risposte entro circa 100 ms.
- **hit-stop** di 0.05-0.12 s tramite `CustomTimeDilation` sugli attori coinvolti, non con la global time dilation, così input e UI restano reattivi; il ripristino usa un timer in tempo reale
- camera shake con il modello a trauma (`trauma²`, che decade), tramite Camera Shake/Impulse sulla camera e mai sul corpo
- knockback, flash del materiale, particelle, SFX con pitch variato
- tre livelli di intensità (small / medium / large): jab, finisher e liberazione
- schivata con i-frame e input buffer durante le animazioni, perché il juice non deve mai bloccare l'input

**AI dei Wolfen:**
- Gli stati sono pochi e chiari (pattuglia → allerta → combattimento → stordito/liberabile). Si può scegliere tra **StateTree** e **Behavior Tree**; per un progetto nuovo StateTree è la scelta naturale, il BT va bene se lo conosci meglio. In entrambi i casi:
  - Blackboard/parametri: `TargetActor`, `LastKnownLocation`, `bStunned`
  - percezione con `AIPerception` (vista), più un Nav Mesh Bounds Volume, senza il quale `MoveTo` fallisce
  - **attack token**: attaccano al massimo 1-2 Wolfen per volta e gli altri girano intorno. È lo standard degli action moderni e rende leggibili i combattimenti di gruppo.
  - utility scoring più avanti, solo per scegliere quale attacco usare
- **Liberazione:** quando il Wolfen va a 0 entra in `Stunned` e diventa liberabile per qualche secondo. Kya avvia l'incantesimo con un input (hold), parte un'animazione con VFX in cui il Wolfen si dissolve e torna Nativ. Poi `OnWolfenFreed` fa crescere il contatore della città.
  È **il momento emotivo** della slice: va trattato con l'intensità "large" del juice e con una piccola pausa musicale.

---

## 5. Camera (`camera-systems`)

- Terza persona: `SpringArmComponent` con collision test (niente compenetrazioni), lag di posizione e rotazione, pitch limitato tra -80° e +45°.
- La camera cambia comportamento in base al movement mode:
  - corsa: standard
  - scivolata: più bassa, FOV più largo, guarda avanti nella direzione della velocità
  - caduta e vento: più lontana e inclinata verso il basso, per vedere dove atterrare
  Le transizioni sono blend morbidi, mai tagli secchi.
- Combattimento: soft lock-on sul bersaglio più vicino, con l'opzione di un lock-on esplicito.
- Respawn al checkpoint: un taglio netto voluto, con lo smoothing azzerato.

## 6. Input (`unreal-enhanced-input`, `input-systems`)

- Input Action: `IA_Move` (Axis2D), `IA_Look` (Axis2D), `IA_Jump`, `IA_Slide`, `IA_Attack`, `IA_Dodge`, `IA_Liberate` (trigger Hold).
- Un IMC per contesto: `IMC_Explore` e `IMC_Combat`, gestiti con le priorità. Niente trigger Combo, che in 5.8 è deprecato: le combo si gestiscono nel combat component.
- Il contesto si aggiunge in `PawnClientRestart`, così regge anche al respawn.
- Gamepad first, deadzone radiale. Più avanti: remapping (Enhanced Input User Settings) e opzione hold/toggle.

---

## 7. Level design della slice (`level-design`)

1. **Prima le metriche**: si misurano in una stanza di test, con una "scala di gap", salto massimo, distanza di planata e velocità della scivolata. Ogni misura del livello deriva da lì.
2. **Blockout**: tutta l'isola in greybox, a scala corretta.
3. **Beat** della slice, con un andamento a dente di sega:

| # | Beat | Tipo | Intensità |
|---|---|---|---|
| 1 | Arrivo sull'isola, corsa e salto | insegna | 0.1 |
| 2 | Prima corrente sicura (cadere qui non uccide) | insegna | 0.2 |
| 3 | Caduta lunga tra le isole, guidata dal vento | test | 0.5 |
| 4 | Vista panoramica sull'accampamento Wolfen | riposo | 0.1 |
| 5 | Scivolata nel canyon verso l'accampamento | test | 0.6 |
| 6 | Scontro con 3-4 Wolfen | climax | 0.9 |
| 7 | Liberazione e Nativs che tornano a casa | riposo / ricompensa | 0.2 |

- Ogni meccanica si **insegna in un posto sicuro** prima di metterla sotto pressione.
- Il giocatore si guida con luce, linee guida e landmark, e il landmark è il bersaglio. Muri invisibili da evitare.
- Checkpoint prima della caduta lunga e prima dello scontro.

---

## 8. UI, salvataggi, audio

- **UI** (`game-ui-ux`): HUD minimale ancorato agli angoli, aggiornato a eventi (dispatcher) e senza polling a ogni tick. Navigazione completa da gamepad nei menu, con focus iniziale sempre impostato. Pause gestita come stack di schermate. Testo minimo 18 px a 1080p su PC.
- **Salvataggi** (`save-systems`): per la slice bastano i checkpoint in memoria. Quando si salverà su disco: `USaveGame` con un campo `Version` fin dal primo giorno, solo dati (ID dei Wolfen liberati, checkpoint, flag) e mai riferimenti ad attori, autosave su uno slot separato.
- **Audio** (`audio-design`): bus `Master ← Music / SFX / Ambience / UI / Voice` con MetaSounds e Submix. Musica adattiva a layer verticali (esplorazione → tensione → combattimento), ducking sotto i momenti chiave, SFX randomizzati per passi e colpi. Loudness di riferimento -24 LUFS.

## 9. Asset e arte (`create-game-assets`)

- **Brief di art direction prima di produrre qualsiasi asset**: forme (isole organiche e morbide da una parte, zone di Brazul spigolose e metalliche dall'altra), palette con i ruoli dei colori (Nativs caldi e saturi, Wolfen freddi e desaturati), materiali, luce.
- Si approva prima **un asset eroe** (un Nativ o un'isola) visto a scala di gioco, poi si producono famiglie coerenti.
- Le immagini AI servono da concept e reference. I modelli 3D vanno puliti in un DCC (topologia, UV, pivot, scala, LOD e collisione verificati).
- Un **manifest** con la provenienza e la licenza di ogni asset. Per la parte legale non deve esistere nessun asset estratto dall'originale.
- Personaggi: Kya e Wolfen si possono partire da MetaHuman o da mesh stilizzate. Animazioni: Mixamo come placeholder, poi motion matching se serve.

## 10. Performance e build (`performance-optimization`, `unreal-packaging`)

- Target 60 FPS. **Prima si misura**: `stat unit`, `stat gpu`, Unreal Insights, su una build Development e non nell'editor.
- Lumen e Nanite sono il default. Attenzione a overdraw delle particelle del vento, vegetazione (usare Nanite foliage e instancing) e Virtual Shadow Maps.
- Pooling per i VFX dei colpi.
- Packaging: Game Default Map impostata, build Development per i test e Shipping per la release, `-run=DataValidation` prima del cook. Va verificato **avviando l'eseguibile**, non basta che il cook sia riuscito.

---

## 11. Roadmap

| Milestone | Contenuto | Esito atteso |
|---|---|---|
| M0 Setup | Progetto UE 5.8 C++, git + LFS, `AKyaCharacter`, Enhanced Input, stanza delle metriche | Kya-capsula che corre e salta con un feel corretto |
| M1 Spike A | Movement mode `WindGlide` + `AWindVolume` + HUD di debug | tieni / butta sul vento |
| M2 Spike B | Movement mode `Slide` + canyon in greybox | tieni / butta sulla scivolata |
| M3 Spike C | Combat component, 1 Wolfen con AI, liberazione, juice | tieni / butta sul combattimento |
| M4 Slice greybox | Riscrittura pulita dei tre pilastri + isola con i beat 1-7 + camera per modalità | slice giocabile dall'inizio alla fine in greybox |
| M5 Playtest | 2-3 persone che non conoscono il gioco, senza spiegazioni | lista dei problemi di flusso e di feel |
| M6 Art pass | Brief → asset eroe → famiglia → Lumen, Niagara, audio | slice "vendibile" come demo |

**Regola d'oro:** niente arte prima di M4 e niente nuove meccaniche prima di M5.

---

## Prossimo passo concreto

M0: creare il progetto UE 5.8 (template Third Person C++), fare l'ossatura di `AKyaCharacter` con i parametri di salto derivati da altezza e tempo, e costruire la stanza delle metriche.
