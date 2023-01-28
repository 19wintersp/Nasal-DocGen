#? Demonstrative module containing all of the processable items.
#?
#? It tests all syntaxes for defining these items, so that the token scanner can
#? be tested in full.

## An example constant.
##
## This is a variable with an assigned value.
##
## @const
## @type num
var myConstant = 123;

(
	## An example constant. (A)
	var myUnpackedConstantA,
	## An example constant. (B)
	var myUnpackedConstantB
) = (1, 2);

var (
	## An example constant. (C)
	myUnpackedConstantC,
	## An example constant. (D)
	myUnpackedConstantD
) = (3, 4);

## An example function.
##
## This is a function taking one argument and returning one value.
##
## @param foo num|str       A useless parameter that does nothing.
## @param ... num|func(int) Extra variable parameters that also do nothing.
## @return    num           The number `123`.
var myFunction = func(foo, bar...) { return 123; };

var myBracketlessFunction = func {};
var myBracelessFunction = func return;
var myEmptyFunction = func;

## An example module.
##
## This is a "module" object with a few random bits in it.
##
## @module
var myModule = {
	## An example constant inside the module.
	myConstant: 123,

	## An example function inside the module.
	myFunction: func,
};

## An example class.
##
## This is a class with a few random bits in it.
##
## @inherit someClass someOtherClass
## @prop foo num A useless property that doesn't exist.
var myClass = {
	## An example property.
	##
	## This is a property defined within the class.
	##
	## @type num
	myProperty: 123,

	## An example constructor.
	##
	## This is a function constructing the class.
	new: func {
		return { parents: [myClass] };
	},

	## An example method.
	##
	## This is a method defined within the class.
	##
	## @return num The number `123`.
	myMethod: func { return 123; },

	## @private
	myPrivateMethod: func,

	## An example method made public explicitly.
	##
	## This is a method defined with an underscore prefix, but made public.
	##
	## @public
	_myPublicMethod: func,
};
