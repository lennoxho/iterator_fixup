# iterator_fixup

Performs fixup of the iterator requirements for given iterator type.

Writing a conformant iterator is difficult. An InputIterator is required to:
1)  satisfy CopyConstructible
2)  satisfy CopyAssignable
3)  satisfy Destructible
4)  lvalues must satisfy Swappable
5)  provide value_type
6)  provide difference_type
7)  provide reference
8)  provide pointer
9)  provide iterator_category
10) be dereferencible via * AND ->
11) be pre AND post incrementible
12) be comparible with == and !=
* the operations have subtle semantics that have to be fulfilled.

Additionally, there are a few implied requirements:
1) operator* should return reference
2) operator-> should return pointer

In my experience, almost none of the InputIterator I've seen fulfills all those requirements.
That is unfortunate, since libraries such as boost::range and boost::algorithm relies heavily on those.

Right now, we will perform fixup of requirements 5-9 and additional requirements 1-2.
This list may expand in the future.
