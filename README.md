# ChènRecPro

Le DAW du **Studio des Chènevières** (Division Digitale & Créative).
Il reprend le meilleur de Cubase, Pro Tools, Logic et Ableton Live, et il est construit sur [Tracktion Engine](https://github.com/Tracktion/tracktion_engine) et JUCE.

Usage interne au studio pour l'instant.

## v0.2 — « Prises »

- **Logo** : la tête de Pistache (photo) dans la barre et l'icône.
- **Inspecteur** à gauche (touche I) : piste, playlist, entrée, clip sélectionné (début, longueur, fondus, gain) et tranche complète avec inserts et fader.
- **Outil intelligent** sur les clips :
  - déplacer, y compris vers une autre piste ;
  - rogner début et fin (bords bas) ;
  - fondus d'entrée et de sortie (coins hauts, courbe à puissance constante).
- **Gain de clip** : Ctrl+Maj+↑/↓ par pas de 1 dB, curseur dans l'inspecteur, valeur affichée sur le clip.
- **Édition** :
  - séparer au curseur avec B ou Ctrl+E ;
  - dupliquer avec Ctrl+D ;
  - menu clic droit sur un clip.
- **Playlists façon Pro Tools** (▾ à côté du nom de piste) : nouvelle, dupliquer, renommer, supprimer, basculer. Elles sont enregistrées dans le projet.
- **Plugins intégrés** : les vraies valeurs s'affichent (dB, ms, ratio, %).
- **v0.1.1** : ASIO (cartes SSL, RME…).
- **v0.1.2** : analyse des plugins hors processus.

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
| B / Ctrl+E | Séparer le clip au curseur |
| Ctrl+D | Dupliquer le clip |
| Ctrl+Maj+↑/↓ | Gain de clip ±1 dB |
| I | Afficher / masquer l'inspecteur |
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
