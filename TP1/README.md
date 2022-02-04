# TP1 - pthread 

Equipe 1 :
- Vincent Commin
- Louis Leenart

# Enoncé

Pour ce TP, nous avons dû parallèliser la recherche de nombres premiers dans plusieurs intervalles. Pour ce faire, nous avons utilisé l'algorithme de [Miller-Rabin](https://fr.wikipedia.org/wiki/Test_de_primalit%C3%A9_de_Miller-Rabin) disponible [ici](https://github.com/cslarsen/miller-rabin) (nous avons récupéré les fichiers `miller-rabin-gmp.hpp` et `miller-rabin-gmp.cpp` et les avons utilisé tel quel moyennant quelques changements que nous expliquerons plus tard). Le fait d'utiliser la version gmp (nommé d'après la bibliothèque `gmp`) nous permet d'exécuter la recherche de nombres premiers sur des nombres codés sur plus de 64 bits.

# Notre approche

Pour parallèliser le programme nous avons testé deux approches différentes :

1. Chaque thread prend le nombre à traiter dans l'intervalle dès que celui-ci à fini de calculer le nombre précédent.
2. Chaque thread à un intervalle qui lui est attribué (utile pour les fichiers avec beaucoup d'intervalles mais inutile pour un fichier avec 2 ou 3 intervalles).

## Modification du code de l'implémentation de Miller Rabin par Christian Stigen Larsen

Pour déterminer si un nombre est premier, nous utilisons l'algorithme de [Miller Rabin](https://fr.wikipedia.org/wiki/Test_de_primalit%C3%A9_de_Miller-Rabin), et afin de nous concentrer sur la parallélisation du calcul, nous avons décidé d'utiliser l'implémentation de Christian Stigen Larsen disponible sur [Github](https://github.com/cslarsen/miller-rabin) qui supporte l'algorithme pour les grands nombres (via la librairie [GMP](https://gmplib.org/)). 

L'implémentation proposée ne supporte pas le multithreading, notamment avec l'utilisation d'une varaible globale pour accéder à un nombre premier (`static gmp_randclass *prng` dans le code initial). Si plusieurs threads venaient à initialiser cette ressource en même temps, nous ferions face à un comportement indéfini. Nous avons donc modifié le code pour que chaque thread puisse accéder à cette ressource. Maintenant, les fonctions qui lancent les threads (`compute_prime` et `compute_prime_improved`) s'occupent de l'initialisation de la ressource (qui est de type `gmp_randclass`) et de donner l'accès aux threads. Aussi, d'après la documentation de [GMP](https://gmplib.org/gmp-man-6.2.1.pdf), l'utilisation de cet objet est thread-safe.

Aussi, l'implémentation de Christian Stigen Larsen de $a^x \mod n$ suivait une implémentation faite par ce dernier, cependant une fonction de la librairie GMP propose déjà une implémentation qui est bien plus performante.

## Algorithme 1

Le premier algorithme de parallélisation, `compute_prime`, consiste à lancer, pour chaque intervale de nombre à traiter, les $n$ thread. Les threads vont prendre chacun leur tour un nombre à traiter dans une pile (contenant tous les nombres de l'interval), appliquer l'algorithme de Miller Rabin sur ce dernier, et recommencer jusqu'à ce qu'il n'y ait plus de nombre à traiter. Cette opération est ensuite répétée pour chaque interval. Le pseudo code est le suivant : 

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
    thread_nombre_premier : vecteur de grands nombres
    TANT QUE Vrai FAIRE 
        PRENDRE mutex_a_traiter
        
        SI (args.a_traiter >= args.max) ALORS // Si il n'y a plus de valeur à traiter
            RENDRE mutex_a_traiter            // Terminer le thread
            QUITTER thread
        FIN SI

        a_traiter : grand nombre = args.a_traiter
        INCREMENTER args.a_traiter

        RENDRE mutex_a_traiter

        est_premier : booléen = calculer_premier(a_traiter, args.tour, args.aleatoire)

        SI (est_premier) ALORS
            AJOUTER a_traiter DANS thread_nombres_premiers
        FIN SI
    FIN TANT QUE 

    SI (TAILLE thread_nombres_premiers > 0) ALORS
        PRENDRE mutex_nombre_premiers
        FUSIONNER thread_nombres_premiers DANS args.nombres_premiers
        RENDRE mutex_nombre_premiers
    FIN SI
FIN thread
```

## Algorithme 2

Après avoir implémenter le premier algorithme, nous avons rapidement remarqué un problème d'optimisation : la création et destruction de threads font appel à des demande système qui sont très couteuses en temps, donc parcourir beaucoup de petits intervals est peu efficace. Nous avons donc trouvé une nouvelle approche. Le programme lance $n$ threads et ces derniers partagent une pile d'intervales à traiter. Chaque thead prend un intervale, 
Après avoir implémenter le premier algorithme, nous avons rapidement remarqué un problème d'optimisation : la création et destruction de threads font appel à des demande système qui sont très couteuses en temps, donc parcourir beaucoup de petits intervals est peu efficace. Nous avons donc trouvé une nouvelle approche. Le programme lance $n$ threads et ces derniers partagent une pile d'intervales à traiter. Chaque thead prend un intervale, dont il va parcourir chacune des valeurs de cet intervale pour déterminer s'il est potentiellement premier ou non (suivant le même algorithme que l'algorithme 1), et reprendre un nouvel intervale quand il a terminé, jusqu'à ce que la pile soit vide. Le pseudo code est le suivant : 

```
mutex_intervalles        : mutex // pour la récupération de l'intervalle à traiter par le thread
mutex_nombre_premiers    : mutex

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
            QUITTER TANT QUE
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
    FIN TANT QUE 
    SI (TAILLE thread_nombre_premier > 0) ALORS
        PRENDRE mutex_nombre_premiers
        FUSIONNER thread_nombre_premier DANS args.nombres_premiers
        RENDRE mutex_nombre_premiers
    FIN SI
FIN thread
```

# Ordinateur utilisé pour les tests de performance

* Modèle : i7-8550U 
* Architecture : x86_64
* OS : Archlinux
* Fréquence CPU : 3.4GHz
* Coeurs physiques : 4
* Coeurs logiques : 8
* Ram : 16 Go, 2400 MT/s

# Résultats expérimentaux

# Analyse des résultats

# Conclusion
