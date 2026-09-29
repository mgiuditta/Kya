# Kya: Dark Lineage — Trama del sequel (design)

Data: 2026-09-29
Stato: approvato in brainstorming, da rivedere

## Obiettivo

Due livelli dello stesso progetto:

- **Pitch**: trama completa del sequel in tre atti, senza vincoli tecnici.
- **Capitolo giocabile**: l'Atto I, progettato per essere realizzabile sul motore decompilato riusando moveset, attori e asset esistenti. Deve avere un inizio e una fine autonomi.

## Canone di partenza (dal primo gioco)

Verificato su fonti pubbliche (Wikipedia, community) e sul codice decompilato:

- Kya e il fratellastro Frank finiscono a Brazelia attraverso un portale attivato da un oggetto trovato in casa.
- I Wolfen sono Nativi trasformati da **Brazul**, che è **Alan, il padre di Kya**, scomparso anni prima.
- Kya ha il potere di **esorcizzare** i Wolfen e riportarli Nativi. Kya viene descritta come **mezza aliena**.
- Kya esorcizza Frank (trasformato in Wolfen), sconfigge Brazul, attiva il portale e si ritrova con Frank su un'**isola deserta**, attaccati da una creatura. È un cliffhanger.
- Non esiste un finale alternativo: liberare più Wolfen sblocca solo contenuti bonus (`UpdateForFreedWolfen`, `src/b-witch/Pause.cpp:2216`, soglie 200/220/240/260).

## Premesse narrative fissate

- **Protagonista**: Kya. Il sequel riparte **esattamente dal cliffhanger dell'isola**.
- **Minaccia**: l'eredità oscura non è finita. Il "Dark Lineage" del titolo è letterale.
- **Twist a due livelli**:
  - *Verità apparente*: sconfiggendo ed esorcizzando il padre, Kya ha assorbito il potere di Brazul (contagio).
  - *Verità nascosta*: il popolo alieno di Alan era il **carceriere di un'entità antica**. La caduta di Alan è stata la fuga dell'entità, che lo ha usato come Brazul. Kya è l'ultima della stirpe dei guardiani.
- **Frank**: non discende da Alan, quindi **l'entità non può corromperlo**. È l'ancora di Kya ed è il primo ad accorgersi che lei sta cambiando.
- **Finale**: Kya accetta la stirpe, la domina e diventa la **guardiana** di Brazelia. È un finale agrodolce e chiuso, ma non definitivo.

## Struttura: tre atti

### Atto I — "Il segno" (capitolo giocabile)

1. **Apertura sull'isola**: la creatura del cliffhanger. Le meccaniche base del primo gioco vengono riprese come tutorial.
2. **Primo potere oscuro**: messa alle strette, Kya perde il controllo e usa l'**Ombra**, fortissima ma con un costo. Frank la vede.
3. **Il ritorno dei Wolfen**: sull'isola, o raggiunta Brazelia dall'isola, compaiono Wolfen nuovi. Qualcuno li sta ricreando anche dopo la caduta di Brazul.
4. **Tensione tra fratelli**: Frank vuole che Kya smetta di usare il potere, ma senza di esso lei non riesce ad andare avanti.
5. **Mini-climax**: il boss è un Nativo trasformato di recente. Kya può vincere senza Ombra (più difficile) o con l'Ombra (più facile, ma lascia un segno visibile). La scelta viene memorizzata per gli atti successivi.
6. **Chiusura**: il Nativo sconfitto riconosce in Kya "il sangue antico". Kya lo attribuisce al contagio. Fine del capitolo.

### Atto II — "Il sangue antico"

1. **Conseguenze**: il segno si allarga. Alcuni Nativi temono Kya, e lei diventa sospetta agli occhi di chi ha salvato. I Nativi liberati nel primo gioco possono restare dalla sua parte, e il loro peso diventa canone.
2. **La fonte**: Kya e Frank scoprono che i nuovi Wolfen sono opera dell'**entità antica**. Brazul non era l'origine del male, solo il suo ultimo strumento.
3. **Midpoint (verità nascosta)**: in un luogo sacro dei Nativi Kya scopre che la stirpe di suo padre era quella dei **carcerieri** dell'entità. Il potere non l'ha "contagiata", ha solo risvegliato qualcosa che aveva già.
4. **Frattura**: Frank si sente tradito e i fratelli si separano. Frank prosegue da solo, con brevi sezioni giocabili.
5. **Tentazione**: l'entità offre a Kya il pieno controllo della stirpe in cambio di Frank. L'atto si chiude con Kya sola, temuta e con la corruzione al massimo.

### Atto III — "La guardiana"

1. **Riconciliazione**: Frank torna. Essendo immune all'entità, è l'unico che può raggiungere Kya quando l'Ombra prende il sopravvento.
2. **Rovesciamento della meccanica**: l'Ombra usata per *proteggere* non corrompe più. Il potere smette di essere un costo e diventa una responsabilità.
3. **Climax**: scontro con l'entità, con Kya e Frank insieme. Chi ha usato poco l'Ombra ottiene un boss più difficile e un finale "pulito". Chi l'ha usata molto deve affrontare la tentazione anche durante lo scontro.
4. **Finale**: Kya accetta la stirpe e diventa la guardiana di Brazelia, finendo il compito fallito dal padre. I Nativi smettono di temerla.

## Nuova meccanica: l'Ombra

- Si carica in combattimento e permette colpi potenziati o una breve forma oscura.
- Ogni uso fa crescere un **indicatore di corruzione**, che influenza l'aspetto di Kya, le reazioni dei Nativi e, nel pitch, la difficoltà del climax e il tono del finale.
- Nell'Atto III, se usata per proteggere, non genera più corruzione.

## Fattibilità del capitolo giocabile

Nel motore **non esiste** una trasformazione del giocatore in Wolfen. L'Atto I riusa:

| Esigenza | Riuso |
|---|---|
| Moveset di Kya | `ActorHero_Fight`, `_Boomy`, `_Wind`, `_Slide`, `_GripClimb`, `_JamGut` |
| Ombra | `ActorHero_Fight` con colpi potenziati e FX scuri; spunti visivi da `ActorWolfenGhost` / `ActorShadows` |
| Nemici | `ActorWolfen*` (Std, Fight, FireArm, Track) |
| Nativi / boss | `ActorNativ`, `ActorWolfen` come base per il Nativo trasformato |
| Frank | `ActorCompanion` |
| Isola | layout nuovo costruito con asset e set esistenti |

Asset realmente nuovi: FX e ricolorazione dell'Ombra, indicatore di corruzione (HUD), layout dell'isola, testi e dialoghi.

## Fuori scopo

- Implementazione tecnica del capitolo giocabile (servirà un piano separato).
- Dialoghi completi, level design dettagliato, design dell'entità.

## Domande aperte

- **Genitorialità di Frank**: va confermato nel gioco che Frank non è figlio di Alan. Se lo fosse, il suo ruolo di "immune" va rivisto (ad esempio: la stirpe si manifesta solo nella parte aliena).
- **Madre di Kya e origine aliena**: da verificare cosa dice esattamente il primo gioco sull'origine "mezza aliena".
- **Isola o Brazelia**: decidere se l'intero Atto I si svolge sull'isola o se l'isola è solo l'apertura.

## Note di ricerca

- **Finale alternativo del primo gioco: non esiste.** La community non ne ha mai visto uno ([forum](https://www.tapatalk.com/groups/kya_dark_lineage/is-there-an-alternate-ending-t264.html)). Nel codice, `UpdateForFreedWolfen` (`src/b-witch/Pause.cpp:2216`) alza un livello di sblocco bonus (`INT_0044982c`) a 200/220/240/260 Wolfen liberati. Lo stesso contatore può salire fino a 5 anche tramite cheat (`src/b-witch/Cheat.cpp:36`). Sblocca la galleria artwork, non cambia la trama.
- **Fonti del canone**: [Wikipedia](https://en.wikipedia.org/wiki/Kya:_Dark_Lineage), [Giant Bomb](https://www.giantbomb.com/wd/3030-20351).
- **Nel motore non c'è una trasformazione del giocatore in Wolfen.** Moveset e attori riutilizzabili sono elencati nella tabella di fattibilità.

## Roadmap del capitolo giocabile

Il capitolo giocabile non si può pianificare come un unico progetto. Gli strumenti esistenti sanno solo *estrarre* asset (KyaBank: BNK → file, G2D → PNG, G3D → glTF). Non esiste un tool che scriva o ricompatti un BNK, non c'è un editor di livelli e diversi formati sono ancora conosciuti solo in parte. Per questo il lavoro è diviso in sotto-progetti, ognuno con la sua spec e il suo piano:

1. **Prototipo Ombra (solo codice)**: spec `2026-09-29-dark-power-prototype-design.md`, piano `docs/superpowers/plans/2026-09-29-dark-power-prototype.md`. **Stato: piano scritto, da eseguire su Windows** (build e test solo Windows).
2. **Pipeline di modding**: scrittura e ricompattazione dei BNK, poi modifica del posizionamento degli attori in un livello esistente. È il prerequisito per qualunque contenuto nuovo. Non iniziato.
3. **Testi e dialoghi**: capire il formato dei testi di gioco per aggiungere battute. Non iniziato.
4. **L'isola e l'Atto I completo**: dipende da 2 e 3. Non iniziato.

Prossimi passi, oltre al sotto-progetto 1: mettere sotto torchio la trama con la skill `grilling`, e usare `systemic-worldbuilding` (installata in `~/.claude/skills/`) per l'entità antica e il popolo dei carcerieri.
