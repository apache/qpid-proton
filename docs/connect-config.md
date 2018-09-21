# Connection Configuration {#connect-config}

Proton clients can read default connection configuration from a
configuration file.

If the environment variable `MESSAGING_CONNECT_FILE` is set, it is the
path to the file. Otherwise the client looks for a file named
`connect.json` in the following locations, using the first one found:

* Current working directory of client process.
* `$HOME/.config/messaging/` - $HOME is user's home directory.
* `$PREFIX/etc/messaging/` - $PREFIX is the prefix where the proton library is installed
* `/etc/messaging/`

The configuration file is in JSON object format. Comments are allowed,
as defined by the [JavaScript Minifier](https://www.crockford.com/javascript/jsmin.html)

The file format is as follows. Properties are shown with their default
values, all properties are optional.

    {
      "scheme": "amqps",   // [string] "amqp" (no TLS) or "amqps"
      "host": "localhost", // [string] DNS or IP address for connection. Defaults to local host.
      "port": "amqps",     // [string] "amqp", "amqps" or port number. Defaults to value of 'scheme'.
      "user": null,        // [string] Authentication user name
      "password": null,    // [string] Authentication password

      "sasl": {
        "enable": true,         // [bool] Enable or disable SASL
        "mechanisms": null,     // [list] List of allowed SASL mechanism names.
                                // If null the library determines the default list.
        "allow_insecure": false // [boolean] Allow mechanisms that send unencrypted clear-text passwords
      },

      // Note: it is an error to have a "tls" object unless scheme="amqps"
      "tls": {
        "cert": null,   // [string] name of client certificate or database
        "key": null     // [string] private key or identity for client certificate
        "ca": null,     // [string] name of CA certificate or database
        "verify": true, // [bool] if true, require a valid cert with matching host name
      }
    }
