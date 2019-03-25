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

    `pecl install pcov`

**Binary distributions**

  * **Microsoft Windows**: use zip archive from [https://windows.php.net/downloads/pecl/releases/pcov/](https://windows.php.net/downloads/pecl/releases/pcov/)

  * **Fedora** 29 and up: use the [php-pecl-pcov](https://apps.fedoraproject.org/packages/php-pecl-pcov) package

    `dnf install php-pecl-pcov`

