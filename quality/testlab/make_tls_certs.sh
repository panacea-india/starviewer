#!/bin/sh
# Generates throwaway certificates for the test PACS with TLS, into tls/, which git ignores.
# Run it with: docker compose run --rm tools sh /lab/make_tls_certs.sh
#
# The server certificate names localhost and not 127.0.0.1, so connecting by 127.0.0.1 tests the
# host name check. other-ca.pem did not sign anything here, so trusting only it tests the CA check.
set -e

mkdir -p /lab/tls
cd /lab/tls

make_ca()
{
    openssl req -x509 -newkey rsa:2048 -nodes -days 825 -subj "/CN=$2" -keyout "$1.key" -out "$1.pem"
}

make_certificate()
{
    openssl req -newkey rsa:2048 -nodes -subj "/CN=$2" -keyout "$1.key" -out "$1.csr"
    printf 'subjectAltName=%s\nextendedKeyUsage=serverAuth,clientAuth\n' "$3" > "$1.ext"
    openssl x509 -req -in "$1.csr" -CA ca.pem -CAkey ca.key -CAcreateserial -days 825 -sha256 -extfile "$1.ext" -out "$1.pem"
    rm "$1.csr" "$1.ext"
}

make_ca ca "Starviewer test lab CA"
make_ca other-ca "Starviewer test lab other CA"
make_certificate server orthanc-tls "DNS:localhost,DNS:orthanc-tls"
make_certificate client STARVIEWER "DNS:starviewer"
cat server.key server.pem > server-combined.pem

# Only the keys of the test lab, but the containers run as other users.
chmod a+r ./*
ls /lab/tls
