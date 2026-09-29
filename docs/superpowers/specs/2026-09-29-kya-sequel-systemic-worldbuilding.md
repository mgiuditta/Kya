# Kya sequel — Worldbuilding sistemico: l'entità e i carcerieri

Data: 2026-09-29
Parte di: `2026-09-29-kya-sequel-plot-design.md`
Metodo: skill `systemic-worldbuilding` (conseguenze a cascata)

## Vincolo guida: deve sembrare il primo gioco

Il sequel deve avere **la stessa resa grafica del primo Kya**, perché la nostalgia è il motivo per cui esiste. Da qui la regola che governa tutto il documento:

> **Il nuovo sta nel significato, non nell'aspetto.** Ogni elemento del lore si mostra con set, attori, FX e palette del primo gioco. Il giocatore rivede cose che conosce e scopre che volevano dire altro.

Regole operative:

1. **Nessuno stile nuovo.** Niente modelli, texture o palette che non esistano già nei BNK originali. L'unica eccezione è il testo.
2. **I luoghi nuovi sono ricombinazioni.** L'isola si costruisce con i set esistenti, ricomposti.
3. **La corruzione si vede con la luce, non con la geometria.** Si usa il meccanismo del prototipo Ombra (`lightingFloat_0xe0` scalato), non un nuovo modello di Kya.
4. **Ogni scena chiave fa eco a una scena del primo gioco** (portale, primo esorcismo, volo sulle correnti, Brazul).
5. **Resa del port fedele.** Gli asset originali vengono renderizzati senza filtri "remaster" di default. Eventuali migliorie grafiche restano opzioni del port e non fanno parte dell'identità del sequel.

Questo vincolo coincide con quello tecnico: il capitolo giocabile può solo riusare asset (vedi la tabella di fattibilità nella spec della trama).

## Livello 1 — Punto di divergenza

Rispetto a quello che il giocatore sapeva alla fine del primo gioco:

- Il popolo di Alan (la **Stirpe**) non ha conquistato Brazelia né ci è finito per caso: ne era il **carcere**. Ha legato un'entità antica a Brazelia e ha tenuto la chiave nel proprio sangue.
- Il carcere **non è un edificio**. È il legame tra il sangue della Stirpe e il mondo stesso. Finché un portatore del sangue resta a Brazelia e "tiene", l'entità dorme.
- Il carcere ha una **chiave**: le **7 rune del medaglione** del primo gioco sono i sigilli. Il **materiale** del carcere è l'**ambra** che a Brazelia si estrae.
- Alan voleva *usare* l'Ospite invece di tenerlo prigioniero, e per questo la Stirpe lo ha **esiliato** (nel primo gioco Brazul dice di essere stato bandito dal suo popolo, poi sterminato). La rottura ha indebolito il legame: l'Ospite ha sterminato la Stirpe e ha preso Alan. **Brazul è ciò che succede a un carceriere quando il prigioniero indossa la sua pelle.** Estraendo l'ambra, Brazul ne ha consumato le mura.
- Alla fine del primo gioco Kya ha riunito le rune e ha usato il portale: **il sigillo si è mosso**. L'Ospite, rimasto senza corpo dopo l'esorcismo di Alan, ha trovato lo spiraglio.

Vocabolario proposto:

| Termine | Chi lo usa | Significato |
|---|---|---|
| **la Stirpe** | Kya, narratore | il popolo di Alan, i carcerieri |
| **il sangue antico** | Nativi anziani | chi porta la chiave nel sangue (Kya) |
| **il Dormiente** | Nativi | l'entità, dal punto di vista di chi l'ha vista solo dormire |
| **l'Ospite** | la Stirpe (iscrizioni) | l'entità, dal punto di vista di chi la tiene dentro |
| **il segno** | tutti | la traccia visibile dell'Ombra su Kya (titolo dell'Atto I) |
| **senza padrone** | Nativi | i Wolfen nati dopo Brazul, che non rispondono a nessuno |

## Livello 2 — Conseguenze dirette

- **L'esorcismo del primo gioco è una tecnica da carceriere.** Kya poteva liberare i Wolfen perché ha il sangue della Stirpe. Rilettura retroattiva senza toccare nulla del primo gioco: *ogni Wolfen liberato nel primo gioco era un piccolo atto di carcere.*
- **La trasformazione in Wolfen è la tecnica dell'Ospite.** Brazul non l'ha inventata, l'ha eseguita. Per questo, anche dopo la sua caduta, nascono nuovi Wolfen.
- **Sconfiggere Brazul ha liberato l'Ospite dal suo ultimo corpo.** La vittoria del primo gioco è anche la causa della crisi del sequel. È il cuore della verità apparente ("ho assorbito il suo potere").
- **L'Ospite cerca un nuovo ospite.** L'unico corpo adatto è un portatore del sangue: Kya. L'Ombra non è un potere che lei ha preso, è l'Ospite che bussa.
- **Frank è inutile all'Ospite.** Non ha il sangue, quindi non può essere né carceriere né ospite. È questo a renderlo l'ancora di Kya.

## Livello 3 — Adattamenti dei sistemi

**Nativi (società e potere)**
- Per generazioni i Nativi hanno vissuto *dentro* un carcere senza saperlo. Molti riti e simboli servivano a mantenere il legame. Il significato si è perso, la forma è rimasta: rune e usanze vengono ripetute senza sapere perché.
- Quando la Stirpe è sparita, ha perso potere chi faceva da tramite (gli anziani, i custodi dei luoghi sacri). Ai giovani restano solo i ricordi di Brazul.

**Wolfen (il "nemico")**
- I Wolfen del primo gioco avevano un comando: Brazul. Quelli *senza padrone* no. Sono più erratici, più isolati e meno organizzati. Come comportamento sono più "Track" che "Fight" in formazione.
- Un Nativo trasformato di recente conserva ancora qualcosa di sé, e questo apre il boss dell'Atto I.

**Resistenze**
- Una parte dei Nativi vede in Kya un'altra Brazul in arrivo: *il sangue antico è lo stesso sangue di Brazul*. Ed è vero.
- Un'altra parte, soprattutto i Wolfen liberati nel primo gioco, la segue. Hanno sentito il sangue che li ha liberati.

**Sfruttamento**
- L'Ospite offre a Kya quello che la Stirpe si è sempre negata: usare il prigioniero invece di tenerlo. È la tentazione dell'Atto II, e ha una logica interna. La Stirpe ha davvero rinunciato a un potere enorme.

## Livello 4 — Evoluzione culturale

- **Lingua**: "sangue antico" nasce come onorifico e diventa un'accusa. Lo stesso termine cambia valore da un atto all'altro, ed è un marcatore linguistico da scrivere nei dialoghi.
- **Etica**: il dilemma della Stirpe è anche quello di Kya. Si può tenere un prigioniero per sempre? Tenerlo dentro di sé è sacrificio o egoismo? Il finale (Kya guardiana) è una risposta: tenere il carcere *scegliendolo*, a differenza di Alan che lo ha abbandonato.
- **Credenze**: i Nativi anziani hanno un culto del Dormiente come presenza da non svegliare, non come dio. I giovani lo considerano una favola.
- **Normalizzazione**: per i Nativi, i Wolfen erano una catastrofe finita con Brazul. I Wolfen senza padrone rompono questa normalità. È la prima scossa dell'Atto I.

## Livello 5 — Intersezioni

| Gruppo | Come vive la rivelazione |
|---|---|
| Nativi anziani | riconoscono il sangue antico, con paura e speranza insieme |
| Nativi giovani | vedono solo una ragazza che fa le cose che faceva Brazul |
| Wolfen liberati nel primo gioco | legame istintivo con Kya: alleati, ma anche "segnati" e quindi sospetti per gli altri Nativi |
| Nativo trasformato di recente (boss Atto I) | ancora in bilico, parla con due voci |
| Frank | escluso dal mistero del sangue, ma è l'unico di cui l'Ospite non può fidarsi né servirsi. I Nativi diffidenti si fidano più di lui che di Kya |
| Kya | appartiene a due mondi (Terra e Stirpe) e, all'inizio, a nessuno dei due |

## Contraddizioni (fonti di conflitto)

1. **Il potere che libera è lo stesso che imprigiona.** L'esorcismo (buono nel primo gioco) e la trasformazione in Wolfen (male) sono due facce del legame della Stirpe. Kya non può rinunciare all'uno senza perdere l'altro.
2. **Vincere Brazul ha peggiorato le cose.** Il trionfo del primo gioco è la causa del sequel. Il giocatore deve sentirsi in colpa di aver vinto.
3. **Chi la teme ha ragione.** I Nativi diffidenti non sono ciechi: il sangue è davvero quello di Brazul. Il conflitto resta aperto perché entrambe le parti hanno ragione.
4. **Il carceriere è anche prigioniero.** La Stirpe era legata a Brazelia quanto l'Ospite. La fuga di Alan è comprensibile. Il finale di Kya, invece, è una scelta presa a occhi aperti.

## Canone visivo: ogni elemento del lore visto con gli occhi del primo gioco

Ogni marcatore visibile deve riusare qualcosa che esiste. La colonna **Stato** distingue ciò che è verificato nel codice da ciò che va controllato estraendo gli asset con KyaBank.

| Elemento del lore | Come si vede | Riuso | Stato |
|---|---|---|---|
| Il segno / corruzione di Kya | Kya si scurisce in proporzione | illuminazione per attore (`lightingFloat_0xe0`), prototipo Ombra | verificato nel codice |
| L'Ombra in azione | colpi con FX scuri | FX esistenti dei Wolfen/Brazul (`ActorBrazul`, `Fx*`) | da scegliere tra gli FX estratti |
| Sigilli del carcere | le 7 rune del medaglione | `ActorRune` | classe verificata, aspetto da verificare con KyaBank |
| Materiale del carcere | ambra, miniere della zona industriale | `ActorAmbre`, set industriali | classe verificata, aspetto da verificare |
| Presenza dell'Ospite | nebbia che si addensa dove l'Ospite è vicino | `ActorFogManager` (zone di nebbia) | verificato nel codice |
| Wolfen senza padrone | stesso modello Wolfen, comportamento isolato | `ActorWolfen_Track` al posto delle formazioni | verificato nel codice |
| Boss finale (Atto III) | Brazul, cioè Alan ripreso | `ActorBrazul` | verificato nel codice |
| Nativo trasformato (boss Atto I) | Wolfen con arena e pattern da boss | `ActorBrazul` deriva da `CActorWolfen`: base per il boss | verificato nel codice |
| L'isola | avamposto in rovina della Stirpe | ricomposizione di set del primo gioco | dipende dalla pipeline di modding |
| Il portale | quello del finale del primo gioco | stessa scena e stessi asset | da localizzare nei BNK |

Scena d'apertura consigliata, tutta nostalgia: l'isola del cliffhanger, la creatura, le correnti di vento (`ActorWind`) e il Boomy. Il giocatore ritrova le meccaniche del primo gioco *prima* di sapere che qualcosa è cambiato.

## Decisioni (grilling del 2026-09-29)

Canone del primo gioco verificato su fonti pubbliche (Wikipedia, HandWiki, Hardcore Gaming 101; alcuni punti solo da estratti di TV Tropes e della fandom wiki): Frank è il fratellastro di Kya, la madre di Kya si chiama anch'essa Kya, Brazul dice di essere stato esiliato dal suo popolo, poi sterminato. La creatura del cliffhanger non si vede mai e il destino di Alan non è detto.

**Lore**
- **L'Ospite è senza forma.** Si manifesta solo attraverso ospiti e con la nebbia.
- **Alan** è stato esiliato perché voleva usare l'Ospite (vedi Livello 1). Dopo l'esorcismo è **vivo ma vuoto**, senza memoria né potere, e non compare nell'Atto I.
- **Frank** è figlio della madre di Kya e di un uomo terrestre, non di Alan: niente sangue della Stirpe, quindi è immune.
- **La madre di Kya** resta fuori dal sequel.

**Atto I**
- **L'isola** è un **avamposto della Stirpe**. Il portale, con il sigillo ormai mosso, ha deviato lì Kya e Frank. Il capitolo finisce riattivando il portale dell'avamposto per saltare verso Brazelia, un'eco della scena del primo gioco.
- **L'isola è abitata** da una piccola comunità di Nativi custodi, discendenti dei servitori della Stirpe. L'Ospite ne ha trasformati alcuni: sono i Wolfen senza padrone del capitolo. Gli altri custodi fanno da PNG.
- **La creatura del cliffhanger** è il primo ospite dell'Ospite dopo Brazul, una bestia di Brazelia posseduta con un modello esistente. Esorcizzarla fa da tutorial.
- **Il boss** è il capo dei custodi, trasformato per ultimo. È un personaggio nuovo: i Nativi noti restano per l'Atto II.
- **Frank** è solo un compagno controllato dall'IA (`ActorCompanion`).
- **La corruzione** si salva come valore continuo (luce su Kya, finale) più un flag che dice se il boss è stato vinto con l'Ombra (battuta di chiusura).
- **I Wolfen liberati nel primo gioco** sono canone fisso: "molti". L'importazione del salvataggio resta un'idea da pitch.

**Atti II e III**
- **Midpoint:** Atea indirizza Kya e Frank verso le **miniere di ambra** della zona industriale. Sotto l'estrazione affiorano le mura del carcere e le scritte della Stirpe.
- **Climax:** l'Ospite si riprende Alan e il boss finale è di nuovo **Brazul**, con lo stesso modello. Kya non lo sconfigge: lo **tiene prigioniero**, ancorata da Frank.

## Potenziale narrativo (punti di massima tensione)

1. **Atto I, fine**: il Nativo boss dice "sangue antico". Kya lo prende per un'accusa (contagio). Il giocatore attento sente un titolo.
2. **Atto II, midpoint**: le rune raccolte in tutto il primo gioco erano i sigilli del carcere, e riunirle ha aperto lo spiraglio. Gli oggetti che il giocatore conosce assumono un significato nuovo: è il massimo della nostalgia.
3. **Atto II, tentazione**: l'Ospite offre a Kya di usare il prigioniero, la cosa che la Stirpe si è sempre proibita.
4. **Atto III, rovesciamento**: di fronte c'è di nuovo Brazul, ma questa volta Kya lo tiene invece di distruggerlo. L'Ombra usata per proteggere è il carcere che funziona di nuovo, non un potere oscuro.

## Da verificare (limiti di questo documento)

- L'aspetto reale di rune, ambra, portale e FX nel primo gioco: va estratto con KyaBank (G2D → PNG, G3D → glTF) prima di assegnare loro un ruolo nel lore.
- Plausibilità e interesse dei singoli rami sono giudizi dell'autore: questo documento traccia le conseguenze, non le sceglie.
