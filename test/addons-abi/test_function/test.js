'use strict';
require('../../common');
var assert = require('assert');

// testing api calls for function
var test_function = require('./build/Release/test_function');


function func1 () {
  return 1;
}
assert.equal(test_function.Test(func1), 1);

function func2 () {
  console.log('hello world!');
}
assert.equal(test_function.Test(func2), null);

function func3 (input) {
  return input + 1;
}
assert.equal(test_function.Test(func3, 1), 2); 

function func4 (input) {
  return func3 (input);
}
assert.equal(test_function.Test(func4, 1), 2);
