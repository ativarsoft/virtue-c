#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <libvirt/libvirt.h>
#include <libvirt/virterror.h>
#include <json-c/json.h>

static int vmDefine()
{
	virConnectPtr conn = NULL;
	virDomainPtr domain = NULL;
	char *xmlDesc = NULL;
	char *uri = NULL;
	int ret = 0;
	//createTemporaryFile(ctx, &executeVirshCreate);
	conn = virConnectOpenAuth(uri, virConnectAuthPtrDefault, 0);
	if (conn == NULL) {
		fprintf(stderr,
			"No connection to hypervisor: %s\n",
			virGetLastErrorMessage());
		return 1;
	}
	uri = virConnectGetURI(conn);
	if (!uri) {
		ret = 1;
		fprintf(stderr,
			"Failed to get URI for hypervisor connection: %s\n",
			virGetLastErrorMessage());
		goto disconnect;
	}
	printf("Connected to hypervisor at \"%s\"\n", uri);
	free(uri);
	domain = virDomainDefineXML(conn, xmlDesc);
disconnect:
	if (0 != virConnectClose(conn)) {
		fprintf(stderr,
			"Failed to disconnect from hypervisor: %s\n",
			virGetLastErrorMessage());
		ret = 1;
	}
	return ret;
}

static int vmStart(const char *name)
{
	virConnectPtr conn = NULL;
	virDomainPtr domain = NULL;
	char *uri = NULL;
	int ret = 0;

	conn = virConnectOpenAuth(uri, virConnectAuthPtrDefault, 0);
	if (NULL == conn) {
		fprintf(stderr,
			"No connection to hypervisor: %s\n",
			virGetLastErrorMessage());
		return 1;
	}
	domain = virDomainLookupByName(conn, name);
	if (!domain) {
		fprintf(stderr,
			"Domain not found: %s\n",
			virGetLastErrorMessage());
		ret = 1;
		goto disconnect;
	}
	if (0 != virDomainCreate(domain)) {
		fprintf(stderr,
			"Failed to start domain: %s\n",
			virGetLastErrorMessage());
		ret = 1;
		goto disconnect;
	}
	virDomainFree(domain);
disconnect:
	if (0 != virConnectClose(conn)) {
		fprintf(stderr,
			"Failed to disconnect from hypervisor: %s\n",
			virGetLastErrorMessage());
		ret = 1;
	}
	return 0;
}

static int vmDestroy(const char *name)
{
	virConnectPtr conn = NULL;
	virDomainPtr domain = NULL;
	char *uri = NULL;
	int ret = 0;

	conn = virConnectOpenAuth(uri, virConnectAuthPtrDefault, 0);
	if (NULL == conn) {
		fprintf(stderr,
			"No connection to hypervisor: %s\n",
			virGetLastErrorMessage());
		return 1;
	}
	domain = virDomainLookupByName(conn, name);
	if (!domain) {
		fprintf(stderr,
			"Domain not found: %s\n",
			virGetLastErrorMessage());
		ret = 1;
		goto disconnect;
	}
	if (0 != virDomainDestroy(domain)) {
		fprintf(stderr,
			"Failed to destroy domain: %s\n",
			virGetLastErrorMessage());
		ret = 1;
		goto disconnect;
	}
	virDomainFree(domain);
disconnect:
	if (0 != virConnectClose(conn)) {
		fprintf(stderr,
			"Failed to disconnect from hypervisor: %s\n",
			virGetLastErrorMessage());
		ret = 1;
	}
	return 0;
}

int route_define()
{
	if (0 != vmDefine())
		return 1;
	return 0;
}

int route_start()
{
	const char *vmname = NULL;
	char *postdata = NULL;
	char *lenenv;
	struct json_object *parsed_json, *name;
	int len;
	int ret = -1;

	lenenv = getenv("CONTENT_LENGTH");
	if (lenenv == NULL) {
		fprintf(stderr, "Could not find CONTENT_LENGTH environment variable.\n");
		return 1;
	}
	len = strtol(lenenv, NULL, 10);
	postdata = malloc(len + 1);
	if (!postdata) {
		fprintf(stderr, "Not enough memory (%d bytes)\n", len);
		return 1;
	}
	memset(postdata, 0, len + 1);
	fread(postdata, 1, len, stdin);
	parsed_json = json_tokener_parse(postdata);
	if (parsed_json == NULL) {
		fputs("Invalid JSON input.\n", stderr);
		ret = 1;
		goto out;
	}
	json_object_object_get_ex(parsed_json, "name", &name);
	vmname = json_object_get_string(name);
	if (vmname == NULL) {
		fputs("Virtual machine name not found.\n", stderr);
		ret = 1;
		goto out;
	}
	fprintf(stderr,
		"Starting virtual machine: %s\n",
		vmname);

	if (0 != vmStart(vmname)) {
		ret = 1;
		goto out;
	}

out:
	free(postdata);
	return ret;
}

int route_destroy()
{
	const char *vmname = NULL;
	char *postdata = NULL;
	char *lenenv;
	struct json_object *parsed_json, *name;
	int len;
	int ret = -1;

	lenenv = getenv("CONTENT_LENGTH");
	if (lenenv == NULL) {
		fprintf(stderr, "Could not find CONTENT_LENGTH environment variable.\n");
		return 1;
	}
	len = strtol(lenenv, NULL, 10);
	postdata = malloc(len + 1);
	if (!postdata) {
		fprintf(stderr, "Not enough memory (%d bytes)\n", len);
		return 1;
	}
	memset(postdata, 0, len + 1);
	fread(postdata, 1, len, stdin);
	parsed_json = json_tokener_parse(postdata);
	if (parsed_json == NULL) {
		fputs("Invalid JSON input.\n", stderr);
		ret = 1;
		goto out;
	}
	json_object_object_get_ex(parsed_json, "name", &name);
	vmname = json_object_get_string(name);
	if (vmname == NULL) {
		fputs("Virtual machine name not found.\n", stderr);
		ret = 1;
		goto out;
	}
	fprintf(stderr,
		"Destroying virtual machine: %s\n",
		vmname);

	if (0 != vmDestroy(vmname)) {
		ret = 1;
		goto out;
	}

out:
	free(postdata);
	return ret;
}

int main()
{
	char *pathinfo, *s;
	int ret = -1;

	pathinfo = getenv("PATH_INFO");
	if (NULL == pathinfo) {
		fputs("Could not find PATH_INFO environment variable.\n",
			stderr);
		ret = 1;
		goto out;
	}
	if (pathinfo[0] != '/') {
		fprintf(stderr, "Invalid path: %s\n", pathinfo);
		ret = 1;
		goto out;
	}
	s = strtok(pathinfo, "/");
	if (NULL == s) {
		fprintf(stderr, "Invalid route: %s\n", pathinfo);
		ret = 1;
		goto out;
	}
	if (strcmp(s, "define") == 0) {
		route_define();
	} else if (strcmp(s, "start") == 0) {
		route_start();
	} else if (strcmp(s, "destroy") == 0) {
		route_destroy();
	} else {
		fprintf(stderr, "Route not found: %s\n", s);
	}
	ret = 0;
out:
	return ret;
}
