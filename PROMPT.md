# Demander de faire l'implémentation sans la documentation

* Chaque fichier devrait se trouver dans la conversation.
    * Si le fichier est complètement nouveau => Code complet
    * Si le fichier existe, indique uniquement les modifications avec du contexte pour que je puisse
      situer cette modification
* Vérifie toujours que tu comprends le fonctionnement des méthodes que tu utilises en cherchant leur
  existence dans le Project Knowledge. Ce que tu crées doit-être compatible avec l'existant et doit
  utiliser les mêmes patterns si possible.
* Tous les commentaires doivent être en anglais britannique.
* Ne produis pas de documentation pour le moment.
* Essaye d'utiliser "auto" autant que possible, sauf si cela rend le code difficilement lisible.
* Si tu ne trouves pas une information, n'invente pas son fonctionnement, cherche dans le project,
  et si tu ne trouves toujours rien, demande-moi.
* Ne produis pas de code tronqué, invalide, non-fonctionnel...
* Il n'est pas nécessaire que l'ensemble soit backward compatible, car l'on est pour le moment en
  développement de la première version, il n'y a pas encore eu de release.
* Travaillons étapes par étapes plutôt qu'en faisant tout le travail en un seul bloc, afin de
  réfléchir et de travailler de façon incrémentale.

# Demander de créer la documentation doxygen

* Réalise une documentation au format doxygen complète, claire et concise de tous les éléments
  non-documentés présents dans le code source. Y compris les champs privés et protected (internal
  documentation).
* Lorsqu'une méthode est utilisée dans plusieurs threads, ajoute un warning dans la documentation.
* Evite les commentaires trop évidents dans la documentation ("Default constructor", ...). Rappel
  toi, cla&ire et concise !
* Toute la documentation et les commentaires doivent être en anglais britannique.