# nx-spa

Proof of concept of homebrew application as Single Page Application for the Nintendo Switch platform.

## Development

This example shows how to serve an Angular or Vue Single Page Application for use on the Nintendo Switch browser applet. To build this software from source as it is, the `Angular CLI` is required. You are free to customize this code in order to use the Single Page Application framework of your choice.

Mandatory requirements are `devkitA64`, `libnx` and `make`.

To install the Angular CLI:

```bash
npm install -g @angular/cli
```

To build the application:

```bash
make
```

To build this software using a Vue frontend, run:

```bash
make vue
```

## Credits

Code mostly borrowed from [BernardoGiordano/Checkpoint](https://github.com/BernardoGiordano/Checkpoint).