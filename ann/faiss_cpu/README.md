# FAISS IVF-Flat on CPU

This small experiment generates random NumPy vectors, builds a FAISS IVF-Flat
index, and finds nearest neighbors. It also uses exact Flat search for comparison.
Edit the constants at the top of `ivf_flat.py` to try different settings.

## Install and run

From the repository root:

```sh
cd ann/faiss_cpu
python3 -m venv .venv
. .venv/bin/activate
python -m pip install -r requirements.txt
python ivf_flat.py
```

## Main ideas

- `nlist`: number of clusters, each with an inverted list of vectors.
- `train(vectors)`: learns the cluster centroids. It does not add the vectors.
- `add(vectors)`: stores vectors in their nearest centroid's list.
- `nprobe`: number of lists searched per query, from 1 to `nlist`. Searching
  more lists usually finds more of the exact neighbors.
- `search(queries, k)`: returns the `k` nearest candidates as two arrays:
  distances first, then vector IDs.

The `IndexFlatL2` quantizer finds the nearest centroids. `IndexIVFFlat` stores
full vectors and searches only the selected lists.

## Output

Each row describes one query. IDs are zero-based row numbers in the generated
dataset. Distances are **squared L2 distances**; smaller means closer.

Approximate and exact neighbors may differ because IVF skips some lists.
The final check temporarily sets `nprobe = nlist` and verifies that IVF matches
exact search on this example, allowing small floating-point distance differences.
