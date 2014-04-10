bemenu
======

Dynamic menu library and client program inspired by dmenu

## License
* [GNU GPLv3 (or any later version)](LICENSE-CLIENT) for client program[s] and
  other sources except library and bindings
* [GNU LGPLv3 (or any later version)](LICENSE-LIB) for library and bindings

## Project Guidelines
* **Coding style**
	* [Linux kernel coding style](https://www.kernel.org/doc/Documentation/CodingStyle)
	  for C sources with following exceptions:
	  * spaces not tabs
	  * indentation size is 4 characters (spaces)
	  * function and variable names are camelCase except for global constants
	* [Standard style](http://legacy.python.org/dev/peps/pep-0008/) for Python
* **Build system** - [CMake](http://www.cmake.org/)
* **API documentation** - [Doxygen](http://www.stack.nl/~dimitri/doxygen/index.html)
  (JavaDoc style comments)
* **Branches**
  * **master** - stable releases (permanent)
  * **develop** - development (permanent), merge back to **master**
  * **dev/topic** - topic related development (temporary), branch off from
    **develop** and merge to **develop**
  * **hotfix/issue** - stable release bugfixes (temporary), branch off from
    **master** and merge to **master** and **develop**
