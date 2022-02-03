# TP1 - pthread 
## Equipe 1
*Louis Leenart & Vincent Commin*

### Enoncé 
Pour ce TP, nous avons dû parallèliser la recherche de nombres premiers dans plusieurs intervalles. Pour ce faire, nous avons utilisé l'algorithme de [Miller-Rabin](https://fr.wikipedia.org/wiki/Test_de_primalit%C3%A9_de_Miller-Rabin) disponible [ici](https://github.com/cslarsen/miller-rabin) (nous avons récupéré les fichiers `miller-rabin-gmp.hpp` et `miller-rabin-gmp.cpp` et les avons utilisé tel quel moyennant quelques changements que nous expliquerons un peu plus tard). Le fait d'utiliser la version gmp (nommé d'après la bibliothèque `gmp`) nous permet d'exécuter la recherche de nombres premiers sur des nombres codés sur plus de 64 bits.

### Notre approche

Pour parallèliser le programme nous avons testé deux approches différentes :

1. Chaque thread prend le nombre à traiter dans l'intervalle dès que celui-ci à fini de calculer le nombre précédent.
2. Chaque thread à un intervalle qui lui est attribué (utile pour les fichiers avec beaucoup d'intervalles mais inutile pour un fichier avec 2 ou 3 intervalles).

#### Algorithme 1

```
mutex_a_traiter         : mutex // pour la récupération du nombre à traiter
mutex_nombre_premiers   : mutex // pour l'ajout d'un nombre premier dans le vecteur des nombres premiers

STRUCTURE arguments_thread
    nombres_premiers : vecteur de grands nombres (mpz_class)
    aleatoire       : (gmp_randclass) // permet de créer de grands nombres aléatoires, nécessaire pour l'algorithme implémenté de Miller-Rabin
    tour            : entier // Nombre d'itérations pour Miller-Rabin
    a_traiter       : grand nombre (mpz_class) // nombre suivant à traiter pour le thread 
    max             : grand nombre (mpz_class) // borne max de l'intervalle
FIN STRUCTURE

thread(args : pointeur vers arguments_thread)
    TANT QUE Vrai FAIRE 
        PRENDRE mutex_a_traiter
        
        SI (args.a_traiter >= args.max) ALORS
            RENDRE mutex_a_traiter
            QUITTER thread
        FIN SI

        a_traiter : grand nombre = args.a_traiter
        INCREMENTER args.a_traiter

        RENDRE mutex_a_traiter

        est_premier : booléen = calculer_premier(a_traiter, args.tour, args.aleatoire)

        SI (est_premier) ALORS
            PRENDRE mutex_nombre_premiers
            AJOUTER a_traiter DANS args.nombres_premiers
            RENDRE mutex_nombre_premiers
        FIN SI
    FIN TANT QUE 
FIN thread
```

#### Algorithme 2

```
mutex_intervalles        : mutex // pour la récupération de l'intervalle à traiter par le thread
mutex_nombre_premiers   : mutex

STRUCTURE arguments_thread
    nombres_premiers : vecteur de grands nombres (mpz_class)
    intervalles     : vecteur de grands nombres contenant les bornes hautes et les bornes basses des intervalles du fichier à traiter 
    tour            : entier // Nombre d'itérations pour Miller-Rabin
FIN STRUCTURE

thread(args : pointeur vers arguments_thread)
    aleatoire : (gmp_randclass)
    thread_nombre_premier : vecteur de grands nombres
    TANT QUE Vrai FAIRE 
        PRENDRE mutex_intervalles
        
        // Si le vecteur contenant les intervalles à une taille < 2, cela signifie qu'il n'y a plus d'intervalles à traiter
        SI (TAILLER args.intervalles < 2) ALORS
            RENDRE mutex_intervalles
            QUITTER thread
        FIN SI

        borne_basse : grand nombre = args.intervalles[0]
        borne_haute : grand nombre = args.intervalles[1]
        SUPPRIMER args.intervalles[0] et args.intervalles[1]

        RENDRE mutex_intervalles

        POUR i ALLANT DE borne_basse A borne_haute FAIRE
            est_premier : booléen = calculer_premier(i, args.tour, aleatoire)
            SI (est_premier) ALORS
                AJOUTER i DANS thread_nombre_premier
            FIN SI
        FIN POUR


        SI (TAILLE thread_nombre_premier > 0) ALORS
            PRENDRE mutex_nombre_premiers
            FUSIONNER args.nombres_premiers DANS args.nombres_premiers
            RENDRE mutex_nombre_premiers
        FIN SI
    FIN TANT QUE 
FIN thread
```