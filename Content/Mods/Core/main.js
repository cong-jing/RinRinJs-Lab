import { bar } from './utils.js';

function foo(a, b) {
    let x = bar(b);
    console.log('In foo: bar(', b, ')=', x);
    return a + bar(x);
}

console.log('In main.js: foo(1,2)=', foo(1, 2));

globalThis.foo = foo;

export { foo, bar };