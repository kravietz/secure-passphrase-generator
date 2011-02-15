# Secure Passphrase Generator (SPG)

SPG is a simple GUI program to generate random passphrases out of dictionaries in various languages. These passphrases are both easier to remember and stronger than most classical passwords composed of alphanumeric characters.

Currently Polish and English dictionaries are included. They are built into the binary, so there is no installer, no configuration files etc. To use, just download and run.

# Functional details

In one run 20 passphrases are generated so that you can choose one that looks best for you. Random separators between words are used to further strengthen these passphrases against bruteforce attacks. Sample:

`Selma-Gybe-Lane-Intake
Award!Lang!Gross!Lift
Castor=Harms=Wound=Yacc`

# Security

With passphrase lengths ranging from 19 to 27 characters classic bruteforce attacks are unfeasible. The only feasible attack is dictionary attack trying all combinations (-) of words from the dictionary. Complexity of this attack is larger than in case of classical alphanumeric password of 8 characters (). Expressed in information entropy, average entropy of each passphrase is 52-56 bits, depending on dictionary size (compared to around 48 of alphanumeric 8 characters password)

# Cryptography details

This program fully relies on Windows Cryptographic API (CryptGenRandom) to generate passhprase. It doesn't implement any own algorithms. On startup, it will try to load strongest cryptographic suite available in host operating system (and will accept only PROV_RSA_AES or PROV_RSA_FULL). Dictionary is randomly indexed using "simple discard method" from NIST SP800-90 (B.5.1.1).

# Release history

* 1.0		initial version, compiled with MinGW
* 1.1		Unicode based, compiled with Visual Studio 2010