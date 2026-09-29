# Prototipo "Ombra" (Dark Power) — design

Data: 2026-09-29
Parte di: `2026-09-29-kya-sequel-plot-design.md`, sotto-progetto 1 del capitolo giocabile.

## Obiettivo

Rispondere a una sola domanda: usare un potere più forte che però "costa" è divertente? Il prototipo gira in un livello esistente, si attiva dal debug menu e non usa asset nuovi.

## Comportamento

- Il toggle "Dark Power" si trova nella finestra *Hero* del debug menu.
- Quando è attivo, i colpi corpo a corpo di Kya fanno danno × moltiplicatore (default 2.0, slider da 1 a 5). Il danno dei nemici non cambia.
- Ogni colpo di Kya che va davvero a segno su un fighter (non parato, non immune) aggiunge corruzione (default +5 per colpo, massimo 100). La corruzione si vede in una progress bar e si azzera con un bottone. I colpi su oggetti che non sono fighter, come le casse, non contano.
- Kya si scurisce in proporzione alla corruzione. La scala di luce va da 1.0 (corruzione 0) a 0.35 (corruzione 100). Con il toggle spento non c'è scurimento.
- Lo stato non viene salvato: si azzera a ogni avvio.

## Punti di aggancio

| Cosa | Dove |
|---|---|
| Logica pura | nuovo `src/port/dark_power.{h,cpp}`, namespace `DarkPower`, sotto `PLATFORM_WIN` |
| Danno | `src/b-witch/ActorFighter.cpp`, in `_SV_HIT_FightCollisionProcessHit`, dopo `hitMsg.damage = hitMsg.damage * this->hitMultiplier;` |
| Colpo a segno | stesso file, nel ramo `if (uVar3 == 0)` dopo `DoMessage(MESSAGE_KICKED)` |
| Scurimento | `port/DebugMenu/src/DebugHero.cpp`, callback `UpdateRegisterer` che imposta `lightingFloat_0xe0 = subObjA->lightingFloat_0x4c * scala` |
| UI | `port/DebugMenu/src/DebugHero.cpp`, sezione nella finestra Hero |
| Test | `port/Test/src/dark_power_tests.cpp` (GoogleTest, logica pura) |

Il filtro "solo eroe" è `this->typeID == ACTOR_HERO_PRIVATE`: `_SV_HIT_FightCollisionProcessHit` è condivisa da tutti i fighter.

## Fuori scopo

- Tinta viola, perché richiederebbe l'override di `ComputeLighting` con una `CLightConfig` propria.
- Boomy, prese e colpi ad area.
- Qualsiasi effetto della corruzione sul gameplay.
- Persistenza delle impostazioni.

## Vincoli

- Il codice in `src/b-witch` va toccato solo sotto `#ifdef PLATFORM_WIN`, senza modificare lo stile del codice decompilato.
- Non si usa `hitMultiplier`, perché `UpdateBracelet` lo sovrascrive.
- Build e test sono solo Windows (VS2022 Clang + Vulkan SDK).
