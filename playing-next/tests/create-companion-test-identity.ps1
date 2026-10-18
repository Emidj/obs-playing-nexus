param([Parameter(Mandatory=$true)][string]$BuildRoot)
$ErrorActionPreference='Stop'
$workspace=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$build=(Resolve-Path -LiteralPath $BuildRoot).Path
$allowed=Join-Path $workspace 'work'
if(-not $build.StartsWith($allowed+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Identity fixture must be under work'}
$directory=Join-Path $build 'companion-test-identity'
New-Item -ItemType Directory -Path $directory -Force | Out-Null
# Ephemeral test identity: no system certificate store and no normal OBS writes.
$rsa=[Security.Cryptography.RSA]::Create(2048)
try {
    $request=[Security.Cryptography.X509Certificates.CertificateRequest]::new('CN=OBS Playing Companion',$rsa,[Security.Cryptography.HashAlgorithmName]::SHA256,[Security.Cryptography.RSASignaturePadding]::Pkcs1)
    $san=[Security.Cryptography.X509Certificates.SubjectAlternativeNameBuilder]::new()
    $san.AddDnsName('OBS Playing Companion')
    $request.CertificateExtensions.Add($san.Build())
    $request.CertificateExtensions.Add([Security.Cryptography.X509Certificates.X509BasicConstraintsExtension]::new($false,$false,0,$true))
    $request.CertificateExtensions.Add([Security.Cryptography.X509Certificates.X509KeyUsageExtension]::new([Security.Cryptography.X509Certificates.X509KeyUsageFlags]::DigitalSignature -bor [Security.Cryptography.X509Certificates.X509KeyUsageFlags]::KeyEncipherment,$true))
    $eku=[Security.Cryptography.OidCollection]::new()
    [void]$eku.Add([Security.Cryptography.Oid]::new('1.3.6.1.5.5.7.3.1'))
    $request.CertificateExtensions.Add([Security.Cryptography.X509Certificates.X509EnhancedKeyUsageExtension]::new($eku,$false))
    $cert=$request.CreateSelfSigned([DateTimeOffset]::UtcNow.AddMinutes(-5),[DateTimeOffset]::UtcNow.AddDays(2))
    try {
        [IO.File]::WriteAllText((Join-Path $directory 'certificate.pem'),$cert.ExportCertificatePem())
        [IO.File]::WriteAllText((Join-Path $directory 'private-key.pem'),$rsa.ExportPkcs8PrivateKeyPem())
    } finally {$cert.Dispose()}
} finally {$rsa.Dispose()}
Write-Output 'Ephemeral TLS test identity generated under work; not a production certificate or installation.'
