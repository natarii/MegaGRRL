#!/usr/bin/env perl
#
# script to pack esp32 firmware images (.bin) into megagrrl-compatible update files (.mgu)
#

use Getopt::Long;
use FileHandle;

use strict;
use warnings;

use constant {
	magic => 'mega',
	ESP_APP_DESC_MAGIC_WORD => 0xabcd5432,
#	esp_app_desc_start => 0x20,
	esp_app_desc_length => 0x100,
	crc32_polynomial => 0xedb88320,
};

my ($app, $updater, $version, $hw, $outfile);
GetOptions(
	'app=s' => \$app,
	'updater=s' => \$updater,
	'version=s' => \$version,
	'hw=s' => \$hw,
	'outfile=s' => \$outfile,
	'help' => sub { help(); }, # so $_[0] is unset
) or help('invalid arguments');

((defined($app) && (-r $app)) || (defined($updater) && (-r $updater)))
	or help('one or both of --app and --updater must be set and readable');

(!(defined($app) && defined($updater) && $app eq $updater))
	or help('--app and --updater must be different files');

(defined($outfile) && ($outfile =~ /.+\.mgu$/) && !(-e $outfile))
	or help('--outfile must be set, end in .mgu, and not exist');

if (defined($hw)) {
	($hw =~ /^[a-z0-9]{1}$/i)
		or help('--hw must be a single character if provided');
}
else {
	print("--hw not specified, defaulting to d (desktop)\n\n");
	$hw = 'd';
}

my ($app_fw, $app_sum, $app_version) = ('', 0, undef);
my ($updater_fw, $updater_sum, $updater_version) = ('', 0, undef);

if (defined($app)) {
	($app_fw, $app_sum, $app_version) = process_firmware('app', $app);
}

if (defined($updater)) {
	($updater_fw, $updater_sum, $updater_version) = process_firmware('updater', $updater);
}

# order of preference: manually specified --version, app esp_app_desc_t header, updater esp_app_desc_t header
my $mgu_version;
if (defined($version)) {
	$mgu_version = $version;
}
elsif (length($app_version)) {
	$mgu_version = $app_version;
}
elsif (length($updater_version)) {
	$mgu_version = $updater_version;
}
else {
	die('unable to extract version from firmware(s) and --version is not set');
}

print("writing packed firmware to $outfile...\n\n");

my $outfh = FileHandle->new($outfile, 'w')
	or die("failed to open $outfile: $!");

$outfh->binmode(); # if we're on windows for some reason

print("mgu_header_t struct:\n");
print("\tmagic -> " . magic, "\n");
print("\thardware_version -> $hw\n");
print("\tver_length -> " . length($mgu_version), "\n");
print("\tapp_size -> " . length($app_fw), "\n");
print("\tupdater_size -> " . length($updater_fw), "\n");
print("\tapp_sum -> " . sprintf('0x%x', $app_sum), "\n");
print("\tupdater_sum -> " . sprintf('0x%x', $updater_sum), "\n");
print("\tversion -> $mgu_version\n\n");

# from megagrrl/firmware/main/mgu.h
#typedef struct {
my $header =
	pack('a4', magic) .					# uint32_t magic;
	pack('A', $hw) .					# char hardware_version;
	pack('C', length($mgu_version)) .	# uint8_t ver_length;
	pack('V', length($app_fw)) .		# uint32_t app_size;
	pack('V', length($updater_fw)) .	# uint32_t updater_size;
	pack('V', $app_sum) .				# uint32_t app_sum;
	pack('V', $updater_sum) .			# uint32_t updater_sum;
#} mgu_header_t;
	pack('A*', $mgu_version);

$outfh->print($header . $app_fw . $updater_fw);

$outfh->close();

print('wrote ' . (length($header) + length($app_fw) + length($updater_fw)) . " bytes to $outfile\n");

exit(0);

sub read_file {
	my $filename = shift;

	my $infh = FileHandle->new($filename, 'r')
		or die("failed to open $filename: $!");

	$infh->binmode();

	my $data = '';
	my $rbuf;

	while (! $infh->eof) {
		$infh->read($rbuf, 1024);
		$data .= $rbuf;
	}

	$infh->close();

	return($data);
}

sub parse_app_desc {
	my $firmware = shift;

	# this makes the assumption that the first occurrence of the magic will always be the esp_app_desc_t header
	my $esp_app_offset = index($firmware, pack('V', ESP_APP_DESC_MAGIC_WORD));

	($esp_app_offset != -1)
		or die('unable to find ESP_APP_DESC_MAGIC_WORD');

	print(sprintf("found ESP_APP_DESC_MAGIC_WORD at offset 0x%x\n\n", $esp_app_offset));

	($esp_app_offset < 0x100) # it should be near the start of the file. 0x100 is arbitrary.
		or die('ESP_APP_DESC_MAGIC_WORD offset >= 0x100, aborting');

	# from esp-idf/components/bootloader_support/include/esp_image_format.h
	#/**
	# * @brief Description about application.
	# */
	#typedef struct {
	my ($esp_magic_word, $esp_secure_version, @esp_reserv1, $esp_version, $esp_project_name, $esp_time, $esp_date, $esp_idf_ver, @esp_app_elf_sha256, @esp_reserv2);
	(
		$esp_magic_word,
		$esp_secure_version,
		@esp_reserv1[0..1],
		$esp_version,
		$esp_project_name,
		$esp_time,
		$esp_date,
		$esp_idf_ver,
		@esp_app_elf_sha256[0..31],
		@esp_reserv2[0..19]
	) = unpack(
		'V' .	#    uint32_t magic_word;        /*!< Magic word ESP_APP_DESC_MAGIC_WORD */
		'V' .	#    uint32_t secure_version;    /*!< Secure version */
		'V2' .	#    uint32_t reserv1[2];        /*!< --- */
		'Z32' .	#    char version[32];           /*!< Application version */
		'Z32' .	#    char project_name[32];      /*!< Project name */
		'Z16' .	#    char time[16];              /*!< Compile time */
		'Z16' .	#    char date[16];              /*!< Compile date*/
		'Z32' .	#    char idf_ver[32];           /*!< Version IDF */
		'C32' .	#    uint8_t app_elf_sha256[32]; /*!< sha256 of elf file */
		'V20' ,	#    uint32_t reserv2[20];       /*!< --- */
		unpack('x' . $esp_app_offset . 'a' . int(esp_app_desc_length), $firmware)
	);
	#} esp_app_desc_t;

	($esp_magic_word == ESP_APP_DESC_MAGIC_WORD)
		or die(sprintf('failed to read esp_app_desc_t struct: bad magic: expected 0x%X, got 0x%X', ESP_APP_DESC_MAGIC_WORD, $esp_magic_word));

	print("esp_app_desc_t struct:\n");
	print("\tmagic_word -> " . sprintf('0x%X', $esp_magic_word), "\n");
	print("\tsecure_version -> " . sprintf('0x%X', $esp_secure_version), "\n");
	print("\tversion -> $esp_version\n");
	print("\tproject_name -> $esp_project_name\n");
	print("\ttime -> $esp_time\n");
	print("\tdate -> $esp_date\n");
	print("\tidf_ver -> $esp_idf_ver\n");
	print("\tapp_elf_sha256 -> ", (map { sprintf('%02x', $_) } @esp_app_elf_sha256), "\n\n");

	return($esp_version); # a lot of work just to get the version :D
}

# pretty slow but portable and compact
sub crc32 {
	my $data = shift;

	my $crc32 = 0xffffffff;

	foreach my $byte (unpack('c*', $data)) {
		my $x = ($crc32 ^ $byte) & 0xff;

		for(my $i = 0; $i < 8; $i++) {
			$x = ($x & 1) ? (($x >> 1) ^ crc32_polynomial) : ($x >> 1);
		}

		$crc32 = $x ^ ($crc32 >> 8);
	}

	return($crc32 ^ 0xffffffff);
}

sub process_firmware {
	my ($type, $filename) = @_;

	my ($fw, $fw_sum, $fw_version);

	print("processing $type firmware from $filename...\n\n");

	$fw = read_file($filename);
	print('read ' . length($fw) . " bytes from $filename\n");

	$fw_sum = crc32($fw);
	print('crc32 is ' . unpack('H*', pack('V', $fw_sum)), "\n");

	$fw_version = parse_app_desc($fw);
	print("$type version is $fw_version\n\n");

	return($fw, $fw_sum, $fw_version);
}

sub help {
	my $error = shift;

	print STDERR ("error: $error\n\n") if (defined($error));

	print STDERR ("$0 usage:\n");
	print STDERR ("--app=<app_file.bin> - (optional) the application portion of the firmware (built without #define FWUPDATE)\n");
	print STDERR ("--updater=<updater_file.bin> - (optional) the updater portion of the firmware (built with #define FWUPDATE)\n");
	print STDERR ("--version=some-firmware-version - (optional) the firmware version. if not specified, it will be preferentially extracted from app_file.bin, or updater_file.bin\n");
	print STDERR ("--hw=<d|p> - (optional) the hardware version, typically one of 'd' for desktop or 'p' for portable. defaults to desktop\n");
	print STDERR ("--outfile=<output_file.mgu> (required) filename to write the packed (.mgu) firmware to\n");
	print STDERR ("--help - this message\n");

	exit(defined($error));
}
