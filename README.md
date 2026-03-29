# Second projet - Performance et Parallélisme (CUDA et OpenMP)

Ce second projet est un compte-rendu de performance sur l'**Exercice 3 - Calcul de fractales sur GPU** du TP4.

Il contient plusieurs implémentations parallèles avec CUDA, ainsi qu'OpenMP et séquentielles, afin de comparer les performances sur le calcul de la fractale de Mandelbrot.

Les programmes sont compilés avec `gcc` et `nvcc`.

--

## Compilation

Depuis la racine du projet :

```bash
make
```

Pour nettoyer :

```bash
make clean
```

---

## Choix du nombre de threads pour la partie OpenMP

OpenMP utilise une variable d’environnement pour déterminer le nombre de threads :

```
export OMP_NUM_THREADS=8
```

Exemple d’exécution :

```
export OMP_NUM_THREADS=16
./mandel_openmp_dynamic.exe
```

---

## Rapports d'analyse des performances

Le dossier `rapport/` contient les documents d'analyse des performances :

- `compte-rendu.*` : version Markdown et PDF du compte rendu sur Mandelbrot
- `donnees.ods` : ensemble des mesures de performances relevées manuellement (temps d'exécution et speedup)