# ChènRecPro

Le DAW du **Studio des Chènevières** (Division Digitale & Créative).
Il reprend le meilleur de Cubase, Pro Tools, Logic et Ableton Live, et il est construit sur [Tracktion Engine](https://github.com/Tracktion/tracktion_engine) et JUCE.

Usage interne au studio pour l'instant.

## v0.1 — « Magnéto »

- **Fenêtre Édition** : pistes audio, règle en mesures, grille à la double-croche, modes Slip / Grille / Shuffle / Spot (façon Pro Tools), formes d'onde, déplacement des clips, curseur de lecture.
- **Transport** : lecture, enregistrement, boucle, métronome, afficheur mesures + timecode + tempo.
- **Enregistrement** : entrées audio assignées aux pistes, armement ●, écoute d'entrée I.
- **Import** : glisser-déposer de fichiers audio, menu Fichier, ou fichiers passés au lancement.
- **Console (F3)** : tranches avec 10 inserts A–J, pan, fader gradué, vu-mètres, Mute / Solo / Rec / Input, master.
- **Plugins façon Pro Tools** : menu d'insertion (★ Chènevières, par catégorie, par fabricant, plugins intégrés), fenêtre de plugin avec barre Piste / Insert / Préréglage / Comparer / Bypass, Ctrl+clic pour le bypass, gestionnaire de plugins (analyse VST3 / AU).
- **Projets** `.chenrec` dans `Documents/ChenRecPro`, avec sauvegarde.
- **Export** du mix en WAV.

## Raccourcis

| Touche | Action |
|---|---|
| Espace | Lecture / Stop |
| R ou * | Enregistrer |
| Entrée / Origine | Retour au début |
| F3 | Console |
| L | Boucle |
| C | Métronome |
| F | Zoom pour tout voir |
| Ctrl+T | Nouvelle piste |
| Ctrl+I | Importer un fichier audio |
| Ctrl+S | Enregistrer le projet |
| Ctrl+Z / Ctrl+Maj+Z | Annuler / Rétablir |
| Ctrl+molette | Zoom horizontal |

## Compiler

```bash
git clone --recursive https://github.com/infomatchoc-droid/chenrecpro
cd chenrecpro
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

GitHub Actions compile Windows et macOS (universel) à chaque envoi. Un tag `vX.Y.Z` publie une Release.

## Licence

GPLv3, comme Tracktion Engine et JUCE en version open source.
