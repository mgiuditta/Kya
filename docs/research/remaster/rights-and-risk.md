# Kya rights holder and fan remaster risk

Research for issue #37, part of the "Wayfinder: Kya remaster feasibility" map (#36).
Researched 2026-09-30. **Not legal advice.** Each claim has a confidence level:
**High** means a primary record confirms it, **Medium** means reputable press or strong
inference, **Low** means an educated guess.

## Short answer

- **Who owns Kya.** Atari SA (the former Infogrames), through its group companies, is by far
  the most likely owner of the *Kya: Dark Lineage* copyright. Eden Games was Atari's wholly
  owned studio when the game shipped. The original Eden company was liquidated in 2013–2014
  with no assets left. Today's "Eden Games" (owned by Animoca Brands) is a new company with no
  known claim to Kya. No public record states the chain of title outright, so this is
  **Medium–High**.
- **Enforcement.** There is no public record of Atari acting against Kya fan work. The Icey1717
  decomp, KyaBank and the Kya DL Files site are all still online. Atari does enforce its rights,
  though. It filed a GitHub DMCA notice in 2023 against a fan *clone* of Missile Command, and it
  now runs a "preservationist publisher" strategy of buying and re-releasing old IP. Risk
  goes up with visibility, with monetisation, and if Atari starts planning its own Kya
  re-release.
- **How comparable projects stay up.** They ship code only and never game data. The player
  supplies their own disc or ROM, and a tool extracts the assets on the player's machine. The
  projects take no money. The big takedowns hit projects that broke one of these rules or
  crossed an owner with a live commercial product (re3/reVC, and the SM64 prebuilt binaries).
- **Commercial path.** A commercial release needs a licence from Atari. Only one route is
  realistic: pitch the working port to Atari, its Infogrames label or its Nightdive studio as a
  ready-made remaster base. Nightdive has done this before: its official *Doom 64* is built on
  a fan port. That route exists but is unlikely, so plan the remaster as a free fan project.
- **Action item for this repo.** `origin/main` appears to track some disc-derived binaries
  (`edSys.irx`, `edSys-ghidra.elf`, `port/Test/data/splash.bin`, `port/Test/data/font.bin`).
  Remove them and scrub them from history before the remaster raises the project's profile.
  See section 5.

## 1. Chain of title

| Claim | Confidence | Source |
|---|---|---|
| Kya was developed by Eden Games (Eden Studios) and published by Atari in 2003 (NA Nov 2003, PAL 2004). | High | Wikipedia pointer, [Kya: Dark Lineage](https://en.wikipedia.org/wiki/Kya:_Dark_Lineage). Retail disc IDs SLUS-20440 / SLES-51473 ([PSX Data Center](https://psxdatacenter.com/psx2/games2/SLUS-20440.html)). The game's own boot code shows an `atari_%c` company splash (`src/b-witch/kya.cpp:2227`). |
| Infogrames took full ownership of Eden Studios in April/May 2002, before Kya shipped. Infogrames renamed itself Atari in 2003. | High | IGN, ["Infogrames Acquires Eden Studios"](https://www.ign.com/articles/2002/04/10/infogrames-acquires-eden-studios), 2002-04-10 |
| The original Eden company (EDEN GAMES SARL, SIREN 414 659 607) entered judicial liquidation on 2013-01-29. The liquidation closed on 2014-09-04 for *insuffisance d'actif* (no assets left). The company is deregistered. | High | French company register via [societe.com](https://www.societe.com/societe/eden-games-414659607.html) |
| Former staff founded today's "Eden Games" in Oct 2013 as a **new** company. It passed through Millennial Esports/Engine Media and was bought by Animoca Brands in 2022 for its racing-game work. | High that it is a separate company. Medium on the dates. | [GamesIndustry.biz, 2022-04-12](https://www.gamesindustry.biz/articles/2022-04-12-eden-games-acquired-by-blockchain-company); [Wikipedia: Eden Games](https://en.wikipedia.org/wiki/Eden_Games) (pointer) |
| Atari, not the studio, held Eden-made game IP. Precedent: in Dec 2016 Atari sold the *Test Drive* brand (including Eden's *Test Drive Unlimited*) to Bigben for €900k and kept the right to exploit existing Test Drive games. | High for the sale. Medium that Kya is held the same way. | [Automaton, 2016-12-15](https://automaton-media.com/articles/newsjp/20161215-36189/); the AMF regulatory filings of 2016-12-08 ([FCCNS150173](https://echanges.dila.gouv.fr/OPENDATA/AMF/CNS/2016/12/FCCNS150173_20161208.pdf)) cover the same transaction |
| Atari Europe SAS (Lyon) registered the US trademarks **KYA** (Reg. 2830657, 2004) and **DARK LINEAGE** (Reg. 2907095, 2004). Both have since been **cancelled**: KYA in 2010 because no Section 8 declaration was filed, DARK LINEAGE by the TTAB in 2006. | High | USPTO TSDR, [sn76451075](https://tsdr.uspto.gov/statusview/sn76451075) and [sn76519026](https://tsdr.uspto.gov/statusview/sn76519026) |
| So the *trademark* protection has lapsed in the US, but the *copyright* in the code, art, audio and text almost certainly still runs. Copyright lasts decades and does not depend on use. The likely owner is an Atari SA group entity (Atari Europe SAS / Atari Interactive). | Medium–High | Inference from the rows above. No public assignment record found. |
| No Kya re-release (PS2 Classics, PC, GOG, Steam) has happened. Atari has made no public statement on the IP since the 2000s. | Medium (absence of evidence) | Web search, 2026-09-30. A fan petition to Atari for a PSN release exists ([change.org](https://www.change.org/p/atari-allow-for-a-kya-dark-lineage-sequel-and-kdl-to-be-released-on-the-psn)). |

**What stays unknown:** whether the Kya copyright sat with Eden SARL and was never assigned
to Atari. The liquidation's "no assets" closure suggests that nothing valuable (such as IP) was
left to sell, which fits Atari holding it (Medium). Sony is only the platform holder. It owns
no part of Kya, but Sony's PS2 runtime libraries linked into the game are Sony code (see
section 5).

## 2. Atari's enforcement record

| Claim | Confidence | Source |
|---|---|---|
| In Aug 2023 an agent acting for Atari SA and Atari Interactive Inc. filed a GitHub DMCA notice against a hobby **Missile Command clone** (`shanesatterfield/MissileCommandClone`). The repo held no Atari code, and the notice asked for the whole repository to be removed. | High | [github/dmca 2023-08-22-atari.md](https://github.com/github/dmca/blob/master/2023/08/2023-08-22-atari.md) |
| That is the only Atari notice in GitHub's public DMCA repo, searched by name on 2026-09-30. The notice targets an IP Atari actively sells (*Missile Command Recharged*). | High for the search result. Medium for the "active product" pattern. | GitHub code search of `github/dmca` |
| In 2011 Atari sent a cease-and-desist to the retro fan site atari2600.org, demanding the domain. | Medium | [Slashdot, 2011-08-22](https://games.slashdot.org/story/11/08/22/1959255/Atari-Targets-Retro-Community-With-Cease-amp-Desist) |
| Kya fan work is untouched: [Icey1717/Kya](https://github.com/Icey1717/Kya) (the decomp this fork is based on), KyaBank/edBank/edFile and [kyadlfiles.github.io](https://kyadlfiles.github.io/) all return HTTP 200 as of 2026-09-30. | High (as of today) | direct check |
| Atari is buying old IP and catalogues. It revived the Infogrames label in April 2024 "to acquire IP and publish games". It bought Nightdive Studios (the remaster specialist) in 2023. It took a majority of Thunderful and bought five Ubisoft IPs in 2025. | High for Infogrames and Nightdive. Medium for the 2025 deals. | [Atari newsroom: Nightdive](https://atari.com/blogs/newsroom/atari-acquires-nightdive-studios); [Gematsu, 2024-04](https://www.gematsu.com/2024/04/atari-revives-infogrames-publishing-label); [Naavik](https://naavik.co/digest/ataris-renewed-ma-strategy/) |

**Reading:** Atari enforces its rights, and it uses a brand-protection agent that files broadly.
There is no sign that it tracks obscure Eden-era titles. The Atari–Nightdive "preservationist"
strategy cuts both ways. It makes Kya a plausible catalogue re-release, and a visible fan
remaster could then clash with it. It also makes Atari the one party that could license or
absorb the work. (Medium)

## 3. Precedents: decomp-based PC ports

| Project | What happened | Confidence | Source |
|---|---|---|---|
| **re3 / reVC** (GTA III/VC decomp) | Take-Two sent a DMCA notice in Feb 2021. The team counter-noticed and the repos came back. Take-Two sued in Sept 2021 (*Take-Two v. Papenhoff*, N.D. Cal. 3:21-cv-06831). The case settled, and the claims against the named defendants were dismissed with prejudice in Apr 2023. Take-Two filed a new notice against re3 mirrors in Apr 2025. `github.com/halpz/re3` now returns HTTP 451. Aggravating factors: GTA is a live, lucrative IP, and Rockstar was shipping its own *Definitive Edition* at the time. | High | [github/dmca 2021-02-19](https://github.com/github/dmca/blob/master/2021/02/2021-02-19-take-two.md), [2025-04-25](https://github.com/github/dmca/blob/master/2025/04/2025-04-25-take-two.md); [CourtListener docket filings](https://storage.courtlistener.com/recap/gov.uscourts.cand.384429/gov.uscourts.cand.384429.1.1.pdf); [TorrentFreak, 2023-04-05](https://torrentfreak.com/take-two-dismisses-claims-against-lead-defendants-in-gta-mods-lawsuit-230405/) |
| **Super Mario 64 PC port** | In May 2020 Nintendo's law firm sent DMCA notices against **prebuilt executables** (which embed ROM assets) and videos, calling them an "unauthorized derivative work". The source decomp `n64decomp/sm64` and `sm64-port/sm64-port` are still up (HTTP 200). | High for the takedowns. High that the repos are still up. | [VGC](https://www.videogameschronicle.com/news/nintendo-takes-action-against-mario-64-pc-port/); [Nintendo Life](https://www.nintendolife.com/news/2020/05/nintendo_cracks_down_on_the_super_mario_64_pc_port) |
| **Ship of Harkinian** (Zelda OoT) | No public takedown. Its README says it "does not include any copyrighted assets". It requires a "legally acquired" ROM, which it checks by hash and converts on the player's machine into an `.o2r` archive. | High for the practice. Medium for "no takedown". | [Shipwright README](https://github.com/HarbourMasters/Shipwright) |
| **OpenGOAL** (Jak & Daxter, PS2, the closest analogue) | No public takedown from Sony. It ships no assets, and the user extracts their own retail ISO. Its README says: "Do not use this decompilation project without the use of your own legally purchased copy." | High for the practice. Medium for "no takedown". | [jak-project README](https://github.com/open-goal/jak-project) |
| **Doom 64 EX → Nightdive Doom 64** | Samuel "Kaiser" Villarreal's 2008 fan reimplementation led to his hiring at Nightdive (now owned by Atari). The official 2020 re-release is largely built on that work. | Medium | [Doom Wiki: Doom 64 EX](https://doomwiki.org/wiki/Doom64_EX); [Shacknews: The story of Nightdive](https://www.shacknews.com/article/141664/the-story-of-nightdive-studios?page=9) |

**The common safe pattern** (Medium–High, drawn from the precedents above):

1. The repo and releases hold **no original data**: no textures, audio, video, text tables or
   binaries from the disc.
2. The player supplies the game. An **installer or first-run tool reads the player's ISO**,
   checks it by hash against known retail builds and extracts or converts the assets locally.
3. **No money**: no donations tied to the build, no Patreon early access, no paid builds, no ads.
4. **No owner trademarks in the branding**: no Atari logo, and the project name should not
   pass for an official product. A clear "unofficial, not affiliated" notice.
5. **Low provocation**: take requests from the owner seriously, and comply quickly if one
   arrives.

Projects that broke point 1 (SM64 binaries) or crossed an owner with an active product
(re3 against the GTA Trilogy) got hit. Projects that kept to all five have not been, so far.

## 4. Is there a commercial licensing path?

- **What a commercial release needs:** a licence from the copyright owner (Atari, as above)
  covering the code (the decomp is a derivative of it), the art, audio and text. It also needs
  checks on third-party parts: licensed music or voice work (Kya's soundtrack credits have not
  been audited), Sony PS2 runtime libraries compiled into the executable, and any middleware
  or codecs. (Medium)
- **Realistic route:** approach Atari's Infogrames label or Nightdive with a working port as a
  ready-made remaster base, the way Doom 64 EX became Nightdive's Doom 64. Atari's stated
  strategy (buying IP, remastering catalogue games) makes this *possible*. Kya is a
  small-audience title, so treat it as unlikely. (Low–Medium)
- **Not a route:** selling the port yourself, or taking donations or crowdfunding tied to it.
  That turns tolerated fan work into the commercial exploitation that brought the suits
  above. (Medium–High)
- **Timing:** any approach to Atari tells them the project exists. Make it only when the port
  is polished enough to pitch, and treat it as a decision for the user, with no contact made without an
  explicit OK.

## 5. Findings about this repository

| Finding | Confidence | Evidence |
|---|---|---|
| `origin/main` tracks `edSys.irx` (22 KB PS2 IOP module; its strings show it links `sysmem`, `sifcmd`, `thbase`) and `edSys-ghidra.elf`. These look like Eden's IOP module taken from the game disc. | Medium | `git ls-tree -r -l origin/main`; `strings edSys.irx` |
| `port/Test/data/splash.bin` and `font.bin` (320 KB each) are decoded texture dumps from the game (`port/Test/src/tests.cpp:466-478` dumps `debugMaterial.image.readBuffer`). They are game-derived data. | Medium–High | as cited |
| The repo is GPL-3.0. That licenses the contributors' own work only. It cannot relicense Atari's underlying rights. The decompiled code itself is the grey zone every decomp project lives in. | High for the licence. Medium for the implication. | `LICENSE.txt` |

**Recommendation:** remove the disc-derived binaries and test dumps before the remaster work
makes the repo more visible. Generate test fixtures from the user's ISO at test time, and
rewrite history if you want them gone from the log (that is a user decision). Also add a
README notice: unofficial, not affiliated with Atari or Eden, requires your own retail copy, no
game data included.

## 6. Legal frame (pointer only)

- **EU** (Atari SA is French): Software Directive 2009/24/EC art. 6 permits decompilation only
  as far as interoperability requires. A full port with redistributed decompiled code goes
  beyond that. **US**: DMCA §1201(f) has a similarly narrow interoperability exemption, and
  fair use for full decomps has never been tested in court (re3 settled). (Medium)
- In practice the risk is civil: takedowns, and at worst a lawsuit. It scales with
  **distribution of original data**, **monetisation**, **visibility** and **conflict with the
  owner's own plans**. (Medium)

## Go/no-go input for the map

A disc-required, non-commercial, asset-free fan remaster is **viable with low-to-moderate
risk**, provided the section 3 pattern is followed and the repo is cleaned first (section 5).
Plan for the possibility of a takedown, and keep the code portable across hosts. Treat the
commercial path as a separate long-shot conversation with Atari. Do not budget for it.
