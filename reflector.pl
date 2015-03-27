use strict;
use warnings;
use Readonly;

Readonly my $RT_CLASS 		=> 1;
Readonly my $RT_ENUM		=> 2;
Readonly my $RT_POINTER		=> 4;
Readonly my $RT_ARRAY		=> 8;
Readonly my $RT_FUNDAMENTAL	=> 16;

my $infile = shift;

my %typeInfos = InitFundamentalTypes();
my @fieldEditInfos = ();
ParseFile($infile, \%typeInfos, \@fieldEditInfos);

my %completeTypes = InitFundamentalTypes();
foreach my $typeName (keys %typeInfos)
{
	BuildType($typeName, \%typeInfos, \%completeTypes);
}

foreach my $completeTypeName (keys %completeTypes)
{
	DumpTypeInfo($completeTypeName, \%completeTypes);
}

my $outfile = $infile;
$outfile =~ s/\.\w+$/.ref/;
open(OUTFILE, ">$outfile") or die "Cannot open $outfile: $!\n";
binmode OUTFILE or die "Cannot set binmode: $!\n";
my $numWrittenTypes = 0;
print OUTFILE pack "L", $numWrittenTypes;	# dummy, prepare slot, true value written later

# Field Edit Infos
my $numFieldEditInfos = $#fieldEditInfos + 1;
print("Saving $numFieldEditInfos field edit info(s)\n");
print OUTFILE pack "L", $numFieldEditInfos;
for my $fieldEditInfo (@fieldEditInfos)
{
	print OUTFILE pack "f", $fieldEditInfo->{limitMin};
	print OUTFILE pack "f", $fieldEditInfo->{limitMax};
	
	WriteString(*OUTFILE, $fieldEditInfo->{help});
}

# Fields
foreach my $completeTypeName (keys %completeTypes)
{
	$numWrittenTypes += WriteBinaryTypeInfo(*OUTFILE, $completeTypeName, \%completeTypes);
}
print("Saved $numWrittenTypes type(s) to $outfile\n");
seek(OUTFILE, 0, 0);
print OUTFILE pack "L", $numWrittenTypes;
close(OUTFILE);

sub Align($$)
{
	my $value = shift;
	my $alignment = shift;
	return ($value + $alignment - 1) & ~($alignment - 1);
}

sub InitFundamentalTypes
{
	my %typeInfos = (
		uint8 => {size => 1, align32 => 1 },
		uint16 => {size => 2, align32 => 2},
		uint32 => {size => 4, align32 => 4},
		uint64 => {size => 8, align32 => 8},
		int8 => {size => 1, align32 => 1},
		int16 => {size => 2, align32 => 2},
		int32 => {size => 4, align32 => 4},
		int64 => {size => 8, align32 => 8},
		float => {size => 4, align32 => 4},
		bool => {size => 1, align32 => 1},
		int => {size => 4, align32 => 4}
	);
	
	while ((my $key, my $value) = each(%typeInfos))
	{
		$value->{reflectionType} = $RT_FUNDAMENTAL;
	}
	
	return %typeInfos;
}

sub ParseFile
{
	my ($fileName, $typeDefs, $fieldEditInfos) = @_;

	open(FILE, "<$fileName") or die "Cannot open $fileName: $!\n";
	
	my $parseMode = 0;
	my $currentOffset = 0;
	my %currentType;
	my $biggestAlignment = 0;
	my $currentEnumValue = 0;
	my $prevParseMode = 0;
	my %currentEnum;
	while(<FILE>)
	{
		if ($parseMode == 0 and /^type\s+(\w+)$/)	# standard type
		{
			$currentType{name} = $1;
			$parseMode = 1;
		}
		elsif ($parseMode == 0 and /^type\s+(\w+)\s*:\s*(\w+)/) # derived type
		{
			$currentType{name} = $1;
			$currentType{baseType} = $2;
			
			$parseMode = 1;
		}
		elsif ($parseMode == 0 and /\s+enum\s+(\w+)$/)
		{
			# Global enum
			$prevParseMode = $parseMode;
			$parseMode = 2;
			$currentEnumValue = 0;
		}
		elsif ($parseMode == 1)
		{
			if (/^end\s*$/)
			{
				$parseMode = 0;
				
				$currentType{size} = 0;
				my $typeName = $currentType{name};
				for my $key (keys %currentType)
				{
					$typeDefs->{$typeName}{$key} = $currentType{$key};
				}
				%currentType = ();
			}
			elsif (/^\s*enum\s+(\w+)/) # Scoped enum
			{
				# Build scoped name
				$currentEnum{name} = $currentType{name} . "::" . $1;				
				$prevParseMode = $parseMode;
				$parseMode = 2;
				$currentEnumValue = 0;
			}
			elsif (/\s*([\w\:]+\**)\s+(\w+)\s+\[\s*(-*\d+\.*\d*)\s*,\s*(\d+\.*\d*)\s*\](?:\s+\"(.+)\")?/)
			{
				# Bounded type, must be fundamental (no ptr, arrays, no class for the time being).
				
				my $fieldEditInfo = {
					limitMin => $3,
					limitMax => $4,
					help => ($5 ? $5 : "")
				};
				my $editInfoIndex = @{$fieldEditInfos};				
				push(@{$fieldEditInfos}, $fieldEditInfo);

				my $fieldDesc = {
					name => $2,
					type => $1,
					offset => 0,
					fieldEditInfoIndex => $editInfoIndex
				};
				push(@{$currentType{fields}}, $fieldDesc);
			}
			elsif (/\s*([\w\:]+\**[\<\w\>]*)\s+(\w+)\s*(?:\[(\d+)\])(?:\s+\"(.+)\")?/)
			{
				# Static array
				
				my $typeName = $1;
				my $fieldName = $2;
				my $fieldHelp = $4;
				my $numElements = 0;
				$numElements = $3;
				$typeName = $typeName . "[" . $numElements . "]";
				
				my $editInfoIndex = 0xFFFF;
				if ($fieldHelp)
				{
					my $fieldEditInfo = {
						limitMin => 0,
						limitMax => 0,
						help => $fieldHelp
					};
					$editInfoIndex = @{$fieldEditInfos};					
					push(@{$fieldEditInfos}, $fieldEditInfo);
				}
				
				my $fieldDesc = {
					name => $fieldName,
					type => $typeName,
					offset => 0,
					numElements => $numElements,
					fieldEditInfoIndex => $editInfoIndex					
				};
				push(@{$currentType{fields}}, $fieldDesc);
			}
			elsif (/\s*([\w\:]+\**[\<\w\>]*)\s+(\w+)(?:\s+\"(.+)\")?/)
			{
				# Other type
			
				my $typeName = $1;
				my $fieldName = $2;
				my $fieldHelp = $3;

				my $editInfoIndex = 0xFFFF;
				if ($fieldHelp)
				{
					my $fieldEditInfo = {
						limitMin => 0,
						limitMax => 0,
						help => $fieldHelp
					};
					$editInfoIndex = @{$fieldEditInfos};					
					push(@{$fieldEditInfos}, $fieldEditInfo);
				}
				
				my $fieldDesc = {
					name => $fieldName,
					type => $typeName,
					offset => 0,
					fieldEditInfoIndex => $editInfoIndex					
				};
				push(@{$currentType{fields}}, $fieldDesc);
			}
			elsif (/\s*vtable/)
			{
				$currentType{hasVtable} = 1;
			}
			elsif (/\s/)
			{
				# Empty line
			}
			else
			{
				print("Syntax error, couldn't parse $_");
			}
		}
		elsif ($parseMode == 2)
		{
			if (/\s*end\s*$/)
			{
				$parseMode = $prevParseMode;
				
				$currentEnum{size} = 4;
				$currentEnum{reflectionType} = $RT_ENUM;
				my $typeName = $currentEnum{name};
				for my $key (keys %currentEnum)
				{
					$typeDefs->{$typeName}{$key} = $currentEnum{$key};
				}
				
				if ($parseMode == 1)
				{
					push(@{$currentType{nestedTypes}}, $typeName);
				}
				
				%currentEnum = ();
			}
			elsif (/\s*(\w+)\s*=\s*(\d+)/)
			{
				push(@{$currentEnum{values}}, $1);
				push(@{$currentEnum{values}}, $2);
				
				$currentEnumValue = $2 + 1;
			}
			elsif (/\s*(\w+)\s*,*$/)
			{
				push(@{$currentEnum{values}}, $1);
				push(@{$currentEnum{values}}, $currentEnumValue);
				
				++$currentEnumValue;
			}
		}
	}
	
	# TODO: handle enum case
	if ($parseMode != 0)
	{
		print("Syntax error: missing 'end' statement, most probably for type $currentType{name}\n");
	}
	
	close(FILE);
}

sub BuildVectorType
{
	my ($vectorTypeName, $typeInfos, $completeTypes, $storedType) = @_;
	
	return if exists $completeTypes->{$vectorTypeName};

	# Hardcoded.
	# Make sure to fix when modifying vector class in C++.
	$completeTypes->{$vectorTypeName}{align32} = 4;
	$completeTypes->{$vectorTypeName}{size} = 16;
	$completeTypes->{$vectorTypeName}{reflectionType} = $RT_CLASS;
	
	my $iterType = $storedType . "*";
	BuildType($iterType, $typeInfos, $completeTypes) unless exists $completeTypes->{$iterType};
	
	my $fieldDescBegin = {
		name => "m_begin",
		type => $iterType,
		offset => 0,
		pointedType => $storedType,
		fieldEditInfoIndex => 0xFFFF
	};
	push(@{$completeTypes->{$vectorTypeName}{fields}}, $fieldDescBegin);
	
	my $fieldDescEnd = {
		name => "m_end",
		type => $iterType,
		offset => 4,
		pointedType => $storedType,
		fieldEditInfoIndex => 0xFFFF
	};
	push(@{$completeTypes->{$vectorTypeName}{fields}}, $fieldDescEnd);
}

sub BuildStaticArrayType
{
	my ($arrayTypeName, $typeInfos, $completeTypes, $storedType, $numElements) = @_;
	
	return if exists $completeTypes->{$arrayTypeName};
	
	BuildType($storedType, $typeInfos, $completeTypes);
	
	$completeTypes->{$arrayTypeName}{align32} = $completeTypes->{$storedType}{align32};
	$completeTypes->{$arrayTypeName}{size} = $completeTypes->{$storedType}{size} * $numElements;
	$completeTypes->{$arrayTypeName}{reflectionType} = $RT_ARRAY;
	$completeTypes->{$arrayTypeName}{dependentType} = $storedType;
	$completeTypes->{$arrayTypeName}{numElements} = $numElements;
}

sub BuildPointerType
{
	my ($ptrTypeName, $typeInfos, $completeTypes, $pointedType) = @_;
	return if exists $completeTypes->{$ptrTypeName};
	
	$completeTypes->{$ptrTypeName}{align32} = 4;
	$completeTypes->{$ptrTypeName}{size} = 4;
	$completeTypes->{$ptrTypeName}{reflectionType} = $RT_POINTER;
	$completeTypes->{$ptrTypeName}{dependentType} = $pointedType;
}

sub BuildEnumType
{
	my ($enumTypeName, $typeInfos, $completeTypes) = @_;
	return if exists $completeTypes->{$enumTypeName};
	
	$completeTypes->{$enumTypeName}{align32} = 4;
	$completeTypes->{$enumTypeName}{size} = 4;
	$completeTypes->{$enumTypeName}{reflectionType} = $RT_ENUM;
	foreach my $enumValue (@{$typeInfos->{$enumTypeName}{values}})
	{
		push @{$completeTypes->{$enumTypeName}{values}}, $enumValue;
	}
}	

sub BuildType
{
	my ($currentTypeName, $typeInfos, $completeTypes, $ownerType) = @_;
	
	return if exists $completeTypes->{$currentTypeName};
	
	if ($currentTypeName =~ /vector\<+(\w+)\>+/)
	{
		BuildVectorType($currentTypeName, $typeInfos, $completeTypes, $1);
		return $currentTypeName;
	}
	
	if ($currentTypeName =~ /(\w+)\[(\d+)\]/)
	{
		BuildStaticArrayType($currentTypeName, $typeInfos, $completeTypes, $1, $2);
		return $currentTypeName;
	}
	
	if ($currentTypeName =~ /(\w+\**)(\*+)$/)
	{
		BuildPointerType($currentTypeName, $typeInfos, $completeTypes, $1);
		return $currentTypeName;
	}

	if (!exists($typeInfos->{$currentTypeName}))
	{
		# Try scoped name
		if ($ownerType)
		{
			$currentTypeName = $ownerType . "::" . $currentTypeName;
		}
		if (!exists($typeInfos->{$currentTypeName}))
		{
			print("*** ERROR: Unknown type: $currentTypeName\n");
			return;
		}
	}
	
	my $hasVtable = exists($typeInfos->{$currentTypeName}{hasVtable});
	my $startOffset = $hasVtable ? 4 : 0;
	if (exists($typeInfos->{$currentTypeName}{baseType}))
	{
		my $baseType = $typeInfos->{$currentTypeName}{baseType};
		$completeTypes->{$currentTypeName}{baseType} = $baseType;

		BuildType($baseType, $typeInfos, $completeTypes);
		
		$startOffset = $completeTypes->{$baseType}{size};
		$completeTypes->{$currentTypeName}{baseTypeOffset} = 0; #$startOffset;
		# We have a vtable, but parent does not, need to add offset.
		if ($hasVtable && exists($typeInfos->{$currentTypeName}{hasVtable}))
		{
			$startOffset += 4;
			$completeTypes->{$currentTypeName}{baseTypeOffset} = 4;
		}
	}
	
	my $typeDef = $typeInfos->{$currentTypeName};
	if (exists($typeDef->{reflectionType}) && $typeDef->{reflectionType} == $RT_ENUM)
	{
		BuildEnumType($currentTypeName, $typeInfos, $completeTypes);
		return $currentTypeName;
	}
	
	my $currentOffset = $startOffset;
	my $biggestAlignment = 1;
	for my $field (@{$typeDef->{fields}})
	{
		my $fieldTypeName = $field->{type};

		$fieldTypeName = BuildType($fieldTypeName, $typeInfos, $completeTypes, $currentTypeName) unless exists $completeTypes->{$fieldTypeName};
		my $fieldTypeInfo = $completeTypes->{$fieldTypeName};
		$field->{type} = $fieldTypeName;
		
		my $fieldAlignment = $fieldTypeInfo->{align32};
		$currentOffset = Align($currentOffset, $fieldAlignment);
		$biggestAlignment = $fieldAlignment if $fieldAlignment > $biggestAlignment;
		
		$field->{offset} = $currentOffset;
		
		push(@{$completeTypes->{$currentTypeName}{fields}}, $field);
		
		$currentOffset += $fieldTypeInfo->{size};
	}
	$completeTypes->{$currentTypeName}{align32} = $biggestAlignment;
	$completeTypes->{$currentTypeName}{size} = Align($currentOffset, $biggestAlignment);
	$completeTypes->{$currentTypeName}{reflectionType} = $RT_CLASS;
	
	return $currentTypeName;
}

sub DumpTypeInfo
{
	my ($currentTypeName, $completeTypes) = @_;

	return if ($completeTypes->{$currentTypeName}{reflectionType} == $RT_FUNDAMENTAL);
	
	print("Type: $currentTypeName, size: $completeTypes->{$currentTypeName}{size} byte(s), ");
	if (exists($completeTypes->{$currentTypeName}{baseType}))
	{
		print("base class: $completeTypes->{$currentTypeName}{baseType}\n");
	}
	else
	{
		print("no base class\n");
	}

	my $typeDef = $completeTypes->{$currentTypeName};	
	for my $field (@{$typeDef->{fields}})
	{
		print("\tName: $field->{name}, type name: $field->{type}, offset: $field->{offset}\n");
		
		print("\t\t$field->{help}\n") if ($field->{help});
	}
}

sub WriteString
{
	my ($FILE, $str) = @_;
	my $strLen = length($str);
	print $FILE pack "S", $strLen;
	print $FILE $str;
}

sub WriteBinaryFields
{
	my ($FILE, $typeDef) = @_;
	my $numFields = @{$typeDef->{fields}};
	print $FILE pack "L", $numFields;
	my $fieldFlags = 0; # TEMPORARY
	my $fieldEditIndex = 0;
	for my $field (@{$typeDef->{fields}})
	{
		print $FILE pack "L", mycrc32($field->{type});
		print $FILE pack "S", $field->{offset};
		print $FILE pack "S", $fieldFlags;
		print $FILE pack "S", $field->{fieldEditInfoIndex};
		WriteString($FILE, $field->{name});
	}
}

sub WriteBinaryEnumElements
{
	my ($FILE, $typeDef) = @_;
	my $numValues = @{$typeDef->{values}};
	# 2 elements per value (name = value)
	print $FILE pack "L", $numValues/2;

	for (my $i = 0; $i < $numValues; $i += 2)
	{
		WriteString($FILE, $typeDef->{values}[$i]);
		print $FILE pack "l", $typeDef->{values}[$i + 1];
	}
}

sub WriteBinaryTypeInfo
{
	my ($FILE, $currentTypeName, $completeTypes) = @_;

	return 0 if ($completeTypes->{$currentTypeName}{reflectionType} == $RT_FUNDAMENTAL);
	
	WriteString($FILE, $currentTypeName);
	my $typeDef = $completeTypes->{$currentTypeName};	
	print $FILE pack "L", $typeDef->{size};
	print $FILE pack "L", $typeDef->{reflectionType};
	
	if ($typeDef->{reflectionType} == $RT_CLASS)
	{
		my $baseType = exists($typeDef->{baseType}) ? $typeDef->{baseType} : "";
		my $baseTypeOffset = exists($typeDef->{baseTypeOffset}) ? $typeDef->{baseTypeOffset} : 0;
	
		print $FILE pack "L", mycrc32($baseType);
		print $FILE pack "S", $baseTypeOffset;
		print $FILE pack "L", 0;
		print $FILE pack "L", 0;
		
		WriteBinaryFields($FILE, $typeDef);
	}
	elsif ($typeDef->{reflectionType} == $RT_ARRAY)
	{
		print $FILE pack "L", mycrc32($typeDef->{dependentType});
		print $FILE pack "L", $typeDef->{numElements};
	}
	elsif ($typeDef->{reflectionType} == $RT_POINTER)
	{
		print $FILE pack "L", mycrc32($typeDef->{dependentType});
	}
	elsif ($typeDef->{reflectionType} == $RT_ENUM)
	{
		WriteBinaryEnumElements($FILE, $typeDef);
	}
	
	return 1;
}


# Built-in CRC32 is not 100% compatible with the one my engine uses,
# so we need custom implementation (brute-force copy of my C++ version, basically).
use constant crcTable => [
	0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419,
	0x706AF48F, 0xE963A535, 0x9E6495A3, 0x0EDB8832, 0x79DCB8A4,
	0xE0D5E91E, 0x97D2D988, 0x09B64C2B, 0x7EB17CBD, 0xE7B82D07,
	0x90BF1D91, 0x1DB71064, 0x6AB020F2, 0xF3B97148, 0x84BE41DE,
	0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7, 0x136C9856,
	0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9,
	0xFA0F3D63, 0x8D080DF5, 0x3B6E20C8, 0x4C69105E, 0xD56041E4,
	0xA2677172, 0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B,
	0x35B5A8FA, 0x42B2986C, 0xDBBBC9D6, 0xACBCF940, 0x32D86CE3,
	0x45DF5C75, 0xDCD60DCF, 0xABD13D59, 0x26D930AC, 0x51DE003A,
	0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423, 0xCFBA9599,
	0xB8BDA50F, 0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
	0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D, 0x76DC4190,
	0x01DB7106, 0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F,
	0x9FBFE4A5, 0xE8B8D433, 0x7807C9A2, 0x0F00F934, 0x9609A88E,
	0xE10E9818, 0x7F6A0DBB, 0x086D3D2D, 0x91646C97, 0xE6635C01,
	0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E, 0x6C0695ED,
	0x1B01A57B, 0x8208F4C1, 0xF50FC457, 0x65B0D9C6, 0x12B7E950,
	0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3,
	0xFBD44C65, 0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2,
	0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A,
	0x346ED9FC, 0xAD678846, 0xDA60B8D0, 0x44042D73, 0x33031DE5,
	0xAA0A4C5F, 0xDD0D7CC9, 0x5005713C, 0x270241AA, 0xBE0B1010,
	0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
	0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17,
	0x2EB40D81, 0xB7BD5C3B, 0xC0BA6CAD, 0xEDB88320, 0x9ABFB3B6,
	0x03B6E20C, 0x74B1D29A, 0xEAD54739, 0x9DD277AF, 0x04DB2615,
	0x73DC1683, 0xE3630B12, 0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8,
	0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1, 0xF00F9344,
	0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB,
	0x196C3671, 0x6E6B06E7, 0xFED41B76, 0x89D32BE0, 0x10DA7A5A,
	0x67DD4ACC, 0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5,
	0xD6D6A3E8, 0xA1D1937E, 0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1,
	0xA6BC5767, 0x3FB506DD, 0x48B2364B, 0xD80D2BDA, 0xAF0A1B4C,
	0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55, 0x316E8EEF,
	0x4669BE79, 0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
	0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F, 0xC5BA3BBE,
	0xB2BD0B28, 0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31,
	0x2CD99E8B, 0x5BDEAE1D, 0x9B64C2B0, 0xEC63F226, 0x756AA39C,
	0x026D930A, 0x9C0906A9, 0xEB0E363F, 0x72076785, 0x05005713,
	0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38, 0x92D28E9B,
	0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21, 0x86D3D2D4, 0xF1D4E242,
	0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1,
	0x18B74777, 0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C,
	0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45, 0xA00AE278,
	0xD70DD2EE, 0x4E048354, 0x3903B3C2, 0xA7672661, 0xD06016F7,
	0x4969474D, 0x3E6E77DB, 0xAED16A4A, 0xD9D65ADC, 0x40DF0B66,
	0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
	0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605,
	0xCDD70693, 0x54DE5729, 0x23D967BF, 0xB3667A2E, 0xC4614AB8,
	0x5D681B02, 0x2A6F2B94, 0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B,
	0x2D02EF8D 
];

sub mycrc32
{
	my $str = shift;
	my $value = 0xFFFFFFFF;
	my $strLen = length($str);

	return $value if $strLen == 0;
	
	for my $b(unpack "C*", $str) 
	{
		$value = crcTable->[($value ^ $b) & 0xFF] ^ ($value >> 8);
	}
	$value = crcTable->[($value ^ $strLen) & 0xFF] ^ ($value >> 8);

	return $value;
}
