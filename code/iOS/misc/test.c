int main(int argc, char *argv[])
{
	int i;
	char *ptr = (char *)strtoul(argv[1],0,0);
	for(i = 0; i < 4096; i++)
	{
		if (ptr[i]) printf("%02x ", ptr[i]&0xff);
		break;
	}
	printf("\n");
	return 0;
}
