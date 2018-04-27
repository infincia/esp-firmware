#
# Main Makefile. This is basically the same as a component makefile.
#

COMPONENT_EMBED_TXTFILES := certs/aws-root-ca.pem certs/aws.crt certs/aws.key certs/letsencrypt.chain.pem

COMPONENT_SRCDIRS := . aws homekit
COMPONENT_ADD_INCLUDEDIRS := . aws homekit
