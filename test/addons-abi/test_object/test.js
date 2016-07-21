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

assert.equal(test_object.Test(object, 'hello'), 'world');
assert.deepEqual(test_object.Test(object, 'array'), [1, 94, 'str', 12.321, { test: 'obj in arr' }]);
assert.deepEqual(test_object.Test(object, 'newObject'), { test: 'obj in obj' });



var newObject = test_object.New();
assert.equal(newObject.test_number, 987654321);
assert.equal(newObject.test_string, "test string");
