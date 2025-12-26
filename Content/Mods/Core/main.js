import { bar } from './utils.js';

function foo(a, b) {
    return a + bar(b);
}

globalThis.foo = foo;

export { foo, bar };