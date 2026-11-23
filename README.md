# ChènRecPro

Le DAW du **Studio des Chènevières** (Division Digitale & Créative).
Il reprend le meilleur de Cubase, Pro Tools, Logic et Ableton Live, et il est construit sur [Tracktion Engine](https://github.com/Tracktion/tracktion_engine) et JUCE.

Usage interne au studio pour l'instant.

## v0.15 — « Raccourcis »

- **Raccourcis clavier personnalisables**, comme les Key Commands de Cubase : Édition › Raccourcis clavier et macros… (Ctrl+Alt+K).
  - Toutes les commandes sont rangées par catégorie (Transport, Fenêtres, Édition, Projet), avec une recherche par nom ou par touche.
  - « Ajouter une touche » (ou double-clic), puis appuie sur la touche voulue. Plusieurs touches par commande sont possibles, pavé numérique compris. Si la touche est déjà prise, ChènRecPro propose de la réattribuer.
  - **Préréglages Cubase et Pro Tools** : pavé * pour enregistrer, Alt+X pour couper, F12 ou Ctrl+Espace façon Pro Tools… « Par défaut » et « Tout remettre par défaut » pour revenir en arrière.
- **Macros** : une touche qui enchaîne plusieurs commandes (par exemple : retour au début + métronome + enregistrer).
- Les menus et la barre d'état affichent tes raccourcis à toi. La console (F3) obéit aux mêmes raccourcis.
- Réglages gardés d'une session à l'autre.

## v0.14 — « Freeze »

- **Geler une piste** (Track Freeze de Cubase / Logic) : bouton **flocon** dans l'en-tête, Ctrl+Maj+F, ou Projet › Geler / dégeler.
  - La piste (instrument + inserts jusqu'au fader) est rendue en audio. L'instrument et les plugins sont coupés, ce qui soulage le processeur.
  - Le fader, le pan et les envois post-fader restent actifs : on continue à mixer la piste gelée.
  - Une piste gelée est teintée de bleu (« GELÉE ») et ses clips ne se modifient plus. Re-clic sur le flocon pour la dégeler et la retoucher.
  - Impossible si la sortie va dans une piste Bus (gèle plutôt le Bus) ou si un envoi pré-fader part vers une autre piste : un message l'explique.

## v0.13 — « Dossiers »

- **Pistes dossier**, comme dans Cubase.
  - Sélectionne des pistes (Ctrl+clic sur les en-têtes), puis **Ctrl+Maj+D** ou Projet › Ranger les pistes sélectionnées dans un dossier. Elles se regroupent sous un en-tête de dossier, les unes à la suite des autres.
  - ▾/▸, ou clic dans la zone du dossier : replier/déplier. Replié, le dossier montre en aperçu les clips de toutes ses pistes.
  - **M** et **S** du dossier : muter ou soloter toutes ses pistes d'un coup. Clic sur le nom : sélectionner toutes ses pistes.
  - Double-clic sur le nom : renommer. Clic droit : couleur, tout replier/déplier, défaire le dossier.
  - Un filet de la couleur du dossier marque ses pistes. Le dossier sert à ranger : les sorties ne changent pas (pour un sous-groupe audio, « Nouvelle piste Bus » dans la sortie).

## v0.12 — « Piste d'accords »

- **Piste d'accords** (Chord Track de Cubase), sous la règle : touche **K** ou Projet › Piste d'accords.
  - Clic sur un temps vide : on choisit l'accord dans un menu (fondamentale › couleur, de majeur à m7♭5).
  - Clic sur un accord : on l'écoute. Glisser : on le déplace (calé sur les temps). Clic droit ou double-clic : changer, renverser (C/E, G/B…), écouter, supprimer. Suppr efface l'accord sélectionné.
  - L'écoute passe par la piste instrument sélectionnée, sinon par la première piste instrument du projet.
- Menu de l'en-tête « ♪ ACCORDS » :
  - **Accords → MIDI** : un clip avec basse et accord serré sur la piste instrument.
  - **Les notes suivent les accords** : chaque note du clip MIDI sélectionné va sur la note de l'accord la plus proche.
  - **Détecter les accords** d'un clip MIDI (par demi-mesure, renversements compris).
- Les accords sont enregistrés avec le projet.

## v0.11 — « Arrangement REC »

- **Enregistrer le jeu de la Session dans l'arrangement**, comme le bouton Arrangement Record d'Ableton.
  - Dans la vue Session (Tab), clic sur « ● Arrangement » : le transport démarre s'il était arrêté.
  - Chaque clip ou scène que tu lances s'écrit dans la timeline de sa piste, à l'endroit exact où il a démarré et pour la durée où il a joué. Les boucles se répètent, un clip sans boucle s'arrête à sa fin.
  - Ce qui était déjà dans l'arrangement à ces endroits est remplacé.
  - Re-clic sur le bouton, ou arrêt du transport, pour finir. Ensuite « Retour à l'arrangement » pour réécouter et monter le résultat.

## v0.10 — « Session REC »

- **Enregistrement dans les cases de la vue Session**, comme dans Ableton.
  - Arme une piste avec le ● de son en-tête de colonne : ses cases vides affichent un ● rouge. Clic sur la case pour enregistrer dedans ; re-clic pour finir la prise.
  - À l'arrêt, la prise est calée sur des mesures entières, à partir de la première barre de mesure, et repart aussitôt en boucle. On peut enchaîner une nouvelle prise dans la case suivante pendant que la boucle tourne.
  - Ça marche pour l'audio comme pour le MIDI (pistes instrument).
  - Pendant la prise, seule la case enregistre : les autres pistes armées n'écrivent rien dans l'arrangement. L'écoute de l'entrée reste active.
  - Transport arrêté, l'enregistrement démarre au début de la mesure en cours. Sinon, il démarre tout de suite et la boucle est recalée sur les mesures.

## v0.9 — « Automation »

- **Automation de n'importe quel paramètre de plugin**, en plus du volume et du pan : seuil d'un compresseur, fréquence d'un EQ, mix d'une reverb, niveau des envois…
  - Vue automation (A) : clic sur l'étiquette de la courbe › menu Volume, Pan, Envois, puis chaque insert (A à J) avec tous ses paramètres. Un ● signale les paramètres déjà automatisés, et l'étiquette indique combien d'autres paramètres de la piste sont automatisés.
  - **Bouton AUTO** dans la fenêtre des plugins, comme dans Pro Tools : il propose en tête le **dernier paramètre touché** dans le plugin, puis toute la liste. Le choisir affiche aussitôt sa courbe sur la piste. AUTO reste allumé quand le plugin a de l'automation.
  - Écriture en direct : W allumé, en lecture, tourner un réglage du plugin l'écrit. Ça marche avec les plugins VST3/AU comme avec les plugins intégrés.
  - Effacer l'automation d'un paramètre, ou de toute la piste, depuis le même menu.

## v0.8 — « VariVox »

- **VariVox intégré**, comme VariAudio dans Cubase : double-clic sur un clip audio (ou clic droit › « Corriger la justesse (VariVox)… ») ouvre la zone basse VariVox.
  - La voix est analysée en notes. On retrouve les outils du plugin Chènevières VariVox, avec le même moteur :
    - onglets Hauteur, Segments et Niveau ;
    - potards Quantifier, Redresser, Transitions, Formant, ainsi que Niveler, Cible, Respirations et Sibilantes ;
    - gamme avec « Détecter », versions A/B, annuler/rétablir.
  - Le clip joue la voix corrigée **dans le projet**, en direct : chaque correction est calculée puis écrite dans le dossier « <projet> - VariVox ».
  - « Original » fait entendre la prise d'origine dans le mix ; « Retirer VariVox » remet le clip comme avant.
  - Clic sur une note, transport arrêté : on l'écoute seule.
  - Les corrections sont enregistrées avec le projet et retrouvées à la réouverture. Les clips corrigés portent un badge « VV ».
  - Limite actuelle : pas de clip étiré (warp) ni en boucle.

## v0.7 — « Livraison »

- **Export des stems** (Fichier) : un WAV par piste (inserts, fader et automation compris), dans le dossier de ton choix.
- **Groupes façon Pro Tools** : Ctrl+clic (ou Maj+clic) sur les en-têtes, puis Ctrl+G. Le volume (en relatif), le mute et le solo sont liés, et une pastille de couleur avec la lettre du groupe s'affiche.
  - Ctrl en déplaçant un fader : la piste seule, hors groupe ;
  - Ctrl+Maj+G : dégrouper.
- **Automation du panoramique** : en vue automation (A), un clic sur l'étiquette ▾ VOLUME / ▾ PAN d'une piste bascule entre les deux courbes.
- **Volume dans les en-têtes de piste** : il faut maintenant attraper et glisser la poignée (Maj : réglage fin, double-clic : 0 dB). Un clic à côté ne change plus le niveau.
- **Barre d'état et transport** : plus de chevauchement sur les écrans moins larges.

## v0.6 — « Session »

- **Vue Session façon Ableton Live** (Tab) : une colonne par piste, une ligne par scène.
  - un clic sur un clip le lance, une case vide arrête la piste ;
  - scènes à lancer en un clic, renommables par clic droit, et bouton « + Scène » ;
  - quantification du lancement : immédiat, 1/16 à 8 mesures (1 mesure par défaut) ;
  - « Tout arrêter » et « Retour à l'arrangement » ;
  - **warp** : un fichier audio glissé dans une case est calé au tempo et bouclé ;
  - double-clic dans une case vide d'une piste instrument : nouveau clip MIDI de 4 mesures, ouvert dans l'éditeur clavier.
- **Échap** ferme la fenêtre de plugin active.
- **Inspecteur** : la tranche défile sur les petits écrans, avec un fader de taille normale.
- Le transport ne passe plus sous les modes d'édition sur les petits écrans.

## v0.5 — « Comping »

- **Couloirs de prises** : le bouton ≡ dans l'en-tête de piste (ou le menu playlist) affiche les autres playlists sous la piste.
- **Comping** : glisser dans un couloir remplace la même zone de la playlist active. La sélection est calée sur la grille en mode Grille, et des raccords de 8 ms sont créés automatiquement.
  - ▲ copie la prise entière ;
  - double-clic sur le nom d'un couloir : bascule sur cette playlist.
- **Nouvelle playlist à chaque prise** (menu Transport, activé par défaut) : quand on enregistre sur une piste qui contient déjà quelque chose, l'ancienne version est rangée en couloir et la nouvelle prise part sur une playlist vierge.

## v0.4 — « Mixage »

- **Envois A–E** sur chaque tranche de la console : un clic sur un envoi vide ouvre le menu (bus existants, nouvelle FX réverb, délai ou vide).
  - glisser vers le haut ou la droite : plus fort ; Alt+clic : 0 dB ;
  - clic droit : couper ou supprimer l'envoi ;
  - les envois sont post-fader, comme dans Pro Tools.
- **Pistes FX** : retour d'effet prêt à l'emploi (réverb ou délai 100 % mouillé), aussi dans le menu Projet.
- **Routage** : la sortie d'une piste (E/S ▸) peut aller vers Stéréo Out ou vers une autre piste qui sert de bus. Une nouvelle piste Bus se crée en un clic, et les boucles sont impossibles.
- **Automation du volume** :
  - A affiche les courbes : un clic ajoute un point, on le glisse pour le déplacer, Alt+clic le supprime ;
  - W (ou le bouton W du transport) active l'écriture en direct avec les faders, en mode lecture par défaut.

## v0.3 — « Composition »

- **Pistes instrument** (Ctrl+Maj+T) : choix d'un instrument VST3, ou des instruments intégrés 4OSC et Sampler.
- **Le MIDI suit la sélection**, comme dans Cubase : les claviers maîtres jouent la piste instrument sélectionnée.
- **Enregistrement MIDI** sur la piste armée.
- **MIDI rétrospectif** (Maj+R ou bouton violet du transport) : récupère ce qui a été joué sans enregistrer.
- **Clips MIDI** : un double-clic dans le vide d'une piste instrument crée un clip de 4 mesures. Les notes sont visibles dans la zone de montage.
- **Éditeur clavier** (double-clic sur un clip MIDI) :
  - ajouter, déplacer et allonger des notes, Alt+clic pour supprimer ;
  - couloir des vélocités ;
  - quantification avec Q, transposition avec ↑/↓ (Maj : par octave) ;
  - écoute des notes en les cliquant.
- **Import de fichiers .mid** par glisser-déposer.
- **Sauvegarde automatique** toutes les 60 s.
- Les plugins intégrés qui ont beaucoup de réglages s'ouvrent dans une fenêtre défilante.

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
| K | Piste d'accords |
| Ctrl+Maj+D | Ranger les pistes sélectionnées dans un dossier |
| Ctrl+Maj+F | Geler / dégeler la piste sélectionnée |
| Ctrl+Alt+K | Raccourcis clavier et macros |
| F | Zoom pour tout voir |
| Ctrl+T | Nouvelle piste |
| Ctrl+I | Importer un fichier audio |
| Ctrl+S | Enregistrer le projet |
| Ctrl+Z / Ctrl+Maj+Z | Annuler / Rétablir |
| B / Ctrl+E | Séparer le clip au curseur |
| Ctrl+D | Dupliquer le clip |
| Ctrl+Maj+↑/↓ | Gain de clip ±1 dB |
| I | Afficher / masquer l'inspecteur |
| Ctrl+Maj+T | Nouvelle piste instrument |
| Maj+R | MIDI rétrospectif |
| Q (éditeur) | Quantifier à la double-croche |
| Tab | Basculer Édition / Session |
| Ctrl+G / Ctrl+Maj+G | Grouper / dégrouper |
| A | Afficher l'automation du volume |
| W | Écriture d'automation |
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
