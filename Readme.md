# libcon4m

This is the C library for con4m. The language reference guide is [here](./doc/reference.md).

# tests

## running via dockerfile

To build the dockerfile, run the following command from the repository root:

```
docker build -t libcon4m-tests -f Dockerfile.test .
```

To run the tests, run:

```
docker run libcon4m-tests
```

## running directly

To run the tests directly, the following are required:

- minimum versions clang 17.0.6 or gcc 14.1 or greater
- meson
- ninja

From the repository root, build with:

```
./dev build
```

and run with:

```
./dev run
```
