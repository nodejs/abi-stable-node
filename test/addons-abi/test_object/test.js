'use strict';
require('../../common');
var assert = require('assert');

// Testing api calls for objects
var test_object = require('./build/Release/test_object');


var object = {
  hello: 'world',
  array: [
    1, 94, 'str', 12.321, { test: 'obj in arr' }
  ],
  newObject: {
    test: 'obj in obj'
  }
};

assert.equal(test_object.Get(object, 'hello'), 'world');
assert.deepStrictEqual(test_object.Get(object, 'array'),
                       [ 1, 94, 'str', 12.321, { test: 'obj in arr' } ]);
assert.deepStrictEqual(test_object.Get(object, 'newObject'),
                       { test: 'obj in obj' });

assert(test_object.Has(object, 'hello'));
assert(test_object.Has(object, 'array'));
assert(test_object.Has(object, 'newObject'));

var newObject = test_object.New();
assert(test_object.Has(newObject, 'test_number'));
assert.equal(newObject.test_number, 987654321);
assert.equal(newObject.test_string, 'test string');

// test_object.Inflate increases all properties by 1
var cube = {
  x: 10,
  y: 10,
  z: 10
};

assert.deepStrictEqual(test_object.Inflate(cube), {x: 11, y: 11, z: 11});
assert.deepStrictEqual(test_object.Inflate(cube), {x: 12, y: 12, z: 12});
assert.deepStrictEqual(test_object.Inflate(cube), {x: 13, y: 13, z: 13});
cube.t = 13;
assert.deepStrictEqual(test_object.Inflate(cube), {x: 14, y: 14, z: 14, t: 14});

