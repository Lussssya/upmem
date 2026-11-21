"""A small example of FAISS IVF-Flat search on CPU."""

import faiss
import numpy as np

# Change these values to explore the index.
NUM_VECTORS = 2000
DIMENSION = 16
NUM_QUERIES = 3
NLIST = 8                 # Number of clusters (inverted lists).
NPROBE = 2                # Lists searched for each query; use 1 <= NPROBE <= NLIST.
K = 5                    # Number of neighbors to return.
SEED = 42


def main():
    # FAISS expects rows of float32 vectors.
    rng = np.random.default_rng(SEED)
    vectors = rng.random((NUM_VECTORS, DIMENSION), dtype=np.float32)
    queries = rng.random((NUM_QUERIES, DIMENSION), dtype=np.float32)

    # The quantizer assigns vectors and queries to their nearest centroids.
    quantizer = faiss.IndexFlatL2(DIMENSION)
    index = faiss.IndexIVFFlat(quantizer, DIMENSION, NLIST, faiss.METRIC_L2)

    index.train(vectors)  # Learn the NLIST cluster centroids.
    index.add(vectors)    # Store each vector in its assigned inverted list.
    index.nprobe = NPROBE
    distances, ids = index.search(queries, K)

    print(f"IVF-Flat: {index.ntotal} vectors, nlist={NLIST}, nprobe={NPROBE}")
    print("Approximate neighbor IDs:")
    print(ids)
    print("Approximate squared L2 distances:")
    print(distances)

    # Exact Flat search scans every vector and needs no training.
    exact_index = faiss.IndexFlatL2(DIMENSION)
    exact_index.add(vectors)
    exact_distances, exact_ids = exact_index.search(queries, K)
    print("\nExact neighbor IDs:")
    print(exact_ids)
    print("Exact squared L2 distances:")
    print(exact_distances)

    # A small correctness check: searching every list should match exact search.
    index.nprobe = NLIST
    full_distances, full_ids = index.search(queries, K)
    np.testing.assert_array_equal(full_ids, exact_ids)
    np.testing.assert_allclose(full_distances, exact_distances, rtol=1e-5, atol=1e-6)
    print("\nCheck passed: probing every list matches exact search.")


if __name__ == "__main__":
    main()
