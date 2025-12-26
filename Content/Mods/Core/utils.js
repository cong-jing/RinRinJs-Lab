function bar(x) {
    return x * 2;
}

globalThis.bar = bar;

export { bar };