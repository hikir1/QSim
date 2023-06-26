qsim Quantum Circuit Simulator
------------------------------

qsim is a custom quantum circuit simulator made for
learning, testing, and demoing quantum circuits. It operates on
a maximum of 10 qubits.

It can be compiled on Windows and Unix-based OSes using cl (Visual Studio),
clang/clang++, or gcc/g++.

qsim is very fast. It doesn't use linear algebra to compute the
state, but instead computes it directly. This gives it an
approximate O(n^3) speedup. It is also written in C, doesn't
use any dynamic memory and operates almost exclusively on arrays
of ints. Attempting to multithread it actually slowed it down when
tested on giant circuits (O(100,000) operators). The overhead
of synchronizing was more than the benefit of parallelization. 

Its internal representation allows qsim to compute exact probabilities
in the form of reduced fraction, up to a certain precision (2^-30).

Unfortunately, it does not yet support complex numbers, but will soon.

The operators currently implemented are:
- X    (NOT)
- Z    (Pauli Z)
- H    (Hadamard)
- W    (SWAP)
- M    (measure)
- U<f> (Unitary for binary function <f>)

All operators, except measure, can be multi-controlled. U is a way for the user
to define their own custom operator. It uses its first few qubits as input
for a given binary function, and the output of the binary function is XORed
with the final argument.

The program also has several *commands*:
- draw              (draws the circuit)
- pfunc <f>         (prints the mapping of function <f>)
- state [qubits...] (prints the current state after merging nonspecified qubits, if given)
- prob [quibts...]  (computes and prints the current probability distribution of all or the given qubits)
- pause             (pauses the circuit. Press enter to continue)

The input file format consists of a list of operators, commands, function definitions,
and barriers, each on its own line.

Operators start with the operator symbol, followed by the qubit(s) to apply it to,
followed optionally by a colon and a list of control qubits. Qubits are numbered
from 0 to 9. In addition to listing the qubits explicitly, a range can also
be given in the form <start>..<stop>, where stop is included in the range.

Ex Toffoli gate acting on qubit 4, controlled by 0, 1, and 2:
	X 3 : 0..2

Ex Hadamard gate acting on qubit 9:
	H 9

Hadamard and NOT gates can also be applied to multiple qubits like this:
	H 0..2 5 7
This is equivalent to applying a single Hadamard to every qubit individually.
Hadamards and NOTs are the only single-qubit operators to support this at the moment.
The others will support this in the future.

The special operator U takes an "argument", which is the single letter name of
a predefined function. The function letter should appear immediately adjacent to
the U, with no spaces.

Ex U gate:
	Uf 3..5 : 8

Function definitions begin with a letter in the range a...h, inclusive, and are followed by an
equal sign and a mathematical expression. Currently, all C operators are implemented, including
the ternary operator, as well as the python power operator **. Variables take the form of
letters in the range a...j, where a is the first input qubit and j is the last. These
variables should be used **in order**. For example, if 3 variables are needed, use a, b, and c.
Since only single letter variables are used, multiplication by juxtaposition is supported. All operators
follow C precedence.

Ex functions:
	f = abc ^ d
	g = (a + b + c)**2 & 3 < 3? 7: 8

Barriers can be used to separate parts of the circuit.
This can be done to visually separate the part when the circuit
is drawn. It can also be used to repeat that part of the circuit,
similar to a loop.

A barrier consists of at least one dash, followed by an optional single letter name, with no
whitespace inbetween. This gives the ability to nest barriers. To repeat a section
of the circuit, it must be surrounded by 2 anonymous barriers or barriers of the same name,
and the second barrier takes the number of iterations as an argument. Specifying an iteration
count of 0 is an easy way to comment out a large portion of the circuit.

Ex anonymous barrier, repeating 2 times:
	
	---
	H 0 : 1 2 3
	Z 0
	H 0 : 1 2 3
	--- 2

Ex named barrier:
	-----a

Besides using zero-loop barriers, line comments can be inserted anywhere using the pound sign.
Anything following the pound sign on that line is ignored.

Ex comment:
	#full line comment
	X 3 #comment after gate

White space is arbitrary

The draw command will print the circuit using ascii art. Any part of the circuit
that has already been executed will be drawn in blue. After a measurement operator
has been used, it will display the measured value in green.

Planned for the future:
- increase precision
- support complex numbers and operators
- support more operators
- run commands, and modify circuit dynamically during runtime
- add common debugger commands for setting break points etc.
- pin a drawing of the circuit to the top of the terminal when run from the terminal
- auto focus parts of the circuit depending on what part is running
- allow for more qubits?
- allow for more complex function and variable names?
- constant variables and expressions for more programmatic generation of circuits
