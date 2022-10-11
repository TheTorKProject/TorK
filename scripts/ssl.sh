#!/bin/bash

echo "Generate Bridge Private Key..."
openssl genrsa -des3 -out bridge_private.key 2048

echo "Extracting Bridge Public Key..."
openssl rsa -in bridge_private.key -outform PEM -pubout -out bridge_public.key

echo "Generating CSR..."
openssl req \
       -key bridge_private.key \
       -new -out bridge.csr

echo "Generating Self-Signed Certificate..."
openssl x509 -req -days 365 -in bridge.csr -signkey bridge_private.key -out bridge_cert.crt

echo "Removing the pass phrase..."
openssl rsa -in bridge_private.key -out bridge_private.key

echo "Validating..."
openssl req -text -noout -verify -in bridge.csr
openssl x509 -text -noout -in bridge_cert.crt