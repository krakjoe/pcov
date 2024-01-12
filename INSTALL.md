Requirements
============

  * PHP 7.1+

Installation
============

**From sources**

    git clone https://github.com/krakjoe/pcov.git
    cd pcov
    phpize
    ./configure --enable-pcov
    make
    make test
    make install

Use `develop` branch for development, use `release` branch for last stable released version.

**From PECL**

    pecl install pcov

**Binary distributions**

  * **Microsoft Windows**: use zip archive from [https://windows.php.net/downloads/pecl/releases/pcov/](https://windows.php.net/downloads/pecl/releases/pcov/)

  * **Fedora** 29 and up: use the [php-pecl-pcov](https://packages.fedoraproject.org/pkgs/php-pecl-pcov/php-pecl-pcov/) package

    `dnf install php-pecl-pcov`

* **Ubuntu** Setup Ubuntu PPA [ppa:ondrej/php](https://launchpad.net/~ondrej/+archive/ubuntu/php/)

    `apt install php-pcov`

* **Debian** Setup Debian DPA [packages.sury.org/php](https://packages.sury.org/php/) 

    `apt install php-pcov`
