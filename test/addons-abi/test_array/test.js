'use strict';
require('../../common');
var assert = require('assert');

// Testing api calls for arrays
var test_array = require('./build/Release/test_array');

var array = [
  1,
  9,
  48,
  13493,
  9459324,
  { name: "hello" },
  [
    "world",
    "node",
    "abi"
  ]
];

assert.equal(test_array.Test(array, array.length + 1), "Index out of bound!");

try {
  test_array.Test(array, -2);
}
catch (err){
  assert.equal(err.message, "Invalid index. Expects a positive integer.");
}

array.forEach(function(element, index) {
  assert.equal(test_array.Test(array, index), element);
});


assert.deepEqual(test_array.New(array), array);
