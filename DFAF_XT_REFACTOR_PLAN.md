# DFAF XT Refactor Plan

## Utgångspunkt

`DFAF XT` ska byggas som ett eget projekt i `/Users/eriksode/Projects/DFAFXT/`, med `DFAF` som teknisk referens och arkitekturgrund, inte som en helt ny synthmotor.

Det som är tydligt i referenskoden:

- `DFAFVoice.h` är redan en användbar monofonisk röstmotor med separata envelope-steg, två oscillatorer, FM, noise och tydliga hook-punkter för utbyggnad.
- `PluginProcessor.h` och `PluginProcessor.cpp` har redan ett fungerande patchsystem, parameterlager, presetflöde och host-synkad sequencerdrift.
- `PluginEditor.cpp` visar en sektionerad panelarkitektur där synthdelen och sequencern är åtskilda, och där patchpanelen lever som en egen yta till höger.

Det som är tydligt i XT-skissen:

- XT är `sequencer-first`.
- Sequencern ska dominera nedre halvan av instrumentet.
- Transport/clock ska sitta direkt intill sequencern, inte som en sekundär kontrollgrupp.
- XT-panelen är bredare, mer uppdelad i fasta sektioner och mer "one knob per function" än DFAF.
- Skissen lutar visuellt mot en intern modulationssektion snarare än DFAF:s stora högerspalt med patchbay. Det bör behandlas som en designriktning för XT, inte som ett definitivt bortval av patchning.

## Återanvänd Direkt

### 1. Röstmotor som XT-bas

Återanvänd kärnan i `DFAFVoice.h` som grund för en ny `XTVoice`.

Behåll i första steget:

- envelopeflödet för `VCO`, `VCF` och `VCA`
- oscillatorfaslogiken
- hard sync
- FM mellan oscillatorer
- noise-källan
- velocity-koppling och triggerflöde
- oversamplad oscillatorrendering med decimator

Flytta över med minimala namnbyten först, så att XT startar med ett känt fungerande ljud innan vi expanderar den.

### 2. Processorstruktur

Återanvänd processormönstret från `PluginProcessor.h/.cpp`:

- `AudioProcessorValueTreeState`
- preset/save/load-flöde
- host clock -> step advance
- `processBlock` som central integrationspunkt
- smoothed parameterhantering för cutoff, levels och volume

Det här ger XT en stabil stomme direkt istället för att vi skriver om host-, state- och parameterdelen i onödan.

### 3. Editorprincip

Återanvänd DFAF:s editorarkitektur, inte dess exakta layout:

- egen `LookAndFeel`
- en tydlig `paint()`-driven panel
- en separat `resized()` som placerar ut hela panelen via sektioner
- sektionerade labels i panelen istället för generiska container-widgets

Det här passar XT-skissen mycket bättre än att byta till en mer abstrakt komponentstruktur direkt.

## Refaktorera

### 1. Sequencer: från 8x2 till 16x5

Nuvarande DFAF-sequencer i `DFAFSequencer.h` är en enkel array med:

- `pitch`
- `velocity`
- `numSteps = 8`

XT behöver en ny datamodell:

- `numSteps = 16`
- lanes för `pitch`, `velocity`, `modA`, `modB`, `modC`
- tydlig separation mellan step data, playhead state och GUI binding

Rekommenderad ny struktur:

- `XTSequencer.h`
- `XTSequencerStep` med alla lane-värden för ett steg
- `XTSequencerLane`-enum för UI och routing
- separat metadata för lane-namn, range, defaultvärden och formattering

Det viktiga här är att inte bygga vidare på två hårdkodade slider-arrayer. DFAF:s nuvarande sequencer är för platt för XT.

### 2. Processorparametrar

DFAF:s parameterlayout är idag byggd för:

- 8 pitch-parametrar
- 8 velocity-parametrar
- ett relativt litet synthblock

XT behöver parameterlagret brytas upp i tre grupper:

- voice parameters
- sequencer lane parameters
- modulation/global parameters

Rekommendation:

- skapa namngivning som redan matchar XT-panelen, till exempel `seqPitch01`, `seqVel01`, `seqModA01`
- skapa hjälpfunktioner som genererar lane-parametrar i loop
- undvik att blanda XT-specifika parametrar direkt i DFAF:s gamla ordning

Det här är en viktig refaktor, för annars blir `createParameterLayout()` snabbt svår att underhålla.

### 3. Patch/modulationsarkitektur

DFAF har riktig patchning via `PatchPoint`, `PatchCable` och per-sample summering i processorn. Den grunden är värdefull, men XT-skissen pekar mer mot fasta modulationsdestinationer i panelen.

Rekommenderad XT-riktning:

- behåll den interna patchmotorn som teknisk grund
- exponera XT primärt som en musikalisk modulationssektion
- låt `Mod A/B/C` och `LFO` routas till fasta destinationer i UI
- gör full patchbay till ett senare steg eller en hybridlösning

Kodmässigt betyder det:

- patchmotorn kan fortsätta vara intern signalrouter
- UI:t behöver inte börja med fri kabeldragning
- `PatchPoint` kan senare expanderas för XT-specifika källor och destinationer

Det här ger XT samma familjekänsla som DFAF under huven, men en tydligare och renare panel.

### 4. Editorlayout

Nuvarande DFAF-editor bygger i praktiken på:

- två synthrader
- en 8-stegssequencer längst ned
- högerspalt för patchpanel

XT bör istället refaktoreras till sektioner som matchar skissen:

- `Oscillators / Interaction`
- `Mix / Transient`
- `Filter / Amp / Drive`
- `Modulation`
- `LFO`
- `Transport`
- `Main Sequencer`

Rekommenderad implementation:

- skapa en uppsättning layout-konstanter eller en `LayoutMetrics`-struct
- definiera sektionernas bounds först
- placera sedan knobs, switches, LEDs och step controls relativt sektionen

Detta är viktigare i XT än i DFAF, eftersom panelen är bredare och mycket mer grid-baserad.

## Bygg Nytt

### 1. XT-voice-lager för transient och metal

Nya XT-ljudkällor bör inte pressas in osynligt i nuvarande DFAF-röst utan en tydlig struktur.

Bygg nytt:

- `XTTransientSource` för click/transient-lager
- `XTMetalSource` för metallic/noise-baserad cymbal/hat-karaktär
- enkel mixer före filter/drive/VCA

Målet är att XT:s extra ljudpalett blir tydlig i koden:

- tonal kärna från VCO 1 + VCO 2
- transientlager för attack
- metalliskt lager för hats/cymbal-liknande spektrum

### 2. Modulationsblock

Bygg ett nytt XT-modulationslager som konceptuellt sitter mellan sequencer och voice parameters.

Rekommenderade första byggstenar:

- `XTModDestination`-enum
- `XTModAssignment` för `Mod A/B/C`
- `XTLfo` som egen liten klass
- utility för att mappa lane-värden till parametrar med musikalisk scaling

Det gör att sequencer-rows och LFO kan dela samma destinationsmodell.

### 3. XT-editor

Bygg en ny `XTEditor`, inte en hård modifiering av DFAF:s editor.

Det bör vara en ny filstruktur:

- `Source/XTEditor.h`
- `Source/XTEditor.cpp`
- eventuell `Source/XTLookAndFeel.h`

Skäl:

- XT-panelen har annan visuell hierarki
- sequencern är ett huvudblock, inte ett tillägg längst ned
- transport, mod och LFO är egna panelsektioner i skissen

### 4. Ny project identity

Bygg nytt för XT:

- nytt pluginnamn
- nytt preset namespace
- egen app/preset-mapp
- eget Git-repo

XT ska inte ärva DFAF:s identitet på filsystem- eller presetnivå.

## Rekommenderad Filstruktur För XT

```text
Source/
  XTVoice.h
  XTSequencer.h
  XTModMatrix.h
  XTLfo.h
  XTTransientSource.h
  XTMetalSource.h
  PluginProcessor.h
  PluginProcessor.cpp
  PluginEditor.h
  PluginEditor.cpp
  LookAndFeel.h
```

Praktiskt kan `PluginProcessor.*` och `PluginEditor.*` fortsatt heta så i JUCE-projektet, men de ska innehålla XT-specifik implementation.

## Föreslagen Genomförandeordning

### Fas 1: Bootstrappa XT från DFAF

- skapa nytt XT-projekt i `/Users/eriksode/Projects/DFAFXT/`
- kopiera processor/editor/voice/sequencer som startpunkt
- döp om klassidentitet från `DFAF*` till `XT*` där det är rimligt
- få XT att bygga och låta som DFAF först

### Fas 2: Bygg om sequencern

- ersätt 8-stegsmodellen med 16 steg
- lägg till `Mod A/B/C`
- uppdatera APVTS-parameterlayout
- uppdatera step advance, UI-bindings och displaylogik

### Fas 3: Lägg till nya ljudkällor

- transient/click
- metal generator
- ny mixsektion
- drive-integrering

### Fas 4: Lägg om panelen till XT-layout

- bygg sektioner enligt skissen
- gör sequencern till huvudytan
- flytta transport och clock intill sequencern
- lägg till modblock och LFO-sektion

### Fas 5: Avgör slutlig patch/mod-hybrid

- behåll intern patchmotor oavsett
- besluta om fri patchbay ska exponeras i UI
- om inte: använd patchmotorn under ett mer fast XT-gränssnitt

## Konkret Rekommendation Just Nu

Börja inte med ny DSP eller ny GUI-arkitektur från noll.

Börja med att:

1. porta DFAF till XT som separat kodbas
2. få XT att kompilera i eget repo
3. bryta ut sequencern till en 16-stegsmodell
4. lägga till `Mod A/B/C`
5. därefter utöka ljudkällor och panel

Det ger lägst risk och bäst chans att snabbt få en fungerande `DFAF XT` som verkligen känns som en större och rikare DFAF, inte som ett sidospår med ny motor och fel produktkänsla.
