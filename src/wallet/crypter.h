// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2016 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef BITCOIN_WALLET_CRYPTER_H
#define BITCOIN_WALLET_CRYPTER_H

#include "keystore.h"
#include "serialize.h"
#include "support/allocators/secure.h"

class uint256;

const unsigned int WALLET_CRYPTO_KEY_SIZE = 32;
const unsigned int WALLET_CRYPTO_SALT_SIZE = 8;
const unsigned int WALLET_CRYPTO_IV_SIZE = 16;

/**
 * Private key encryption is done based on a CMasterKey,
 * which holds a salt and random encryption key.
 * 
 * CMasterKeys are encrypted using AES-256-CBC using a key
 * derived using derivation method nDerivationMethod
 * (0 == EVP_sha512()) and derivation iterations nDeriveIterations.
 * vchOtherDerivationParameters is provided for alternative algorithms
 * which may require more parameters (such as scrypt).
 * 
 * Wallet Private Keys are then encrypted using AES-256-CBC
 * with the double-sha256 of the public key as the IV, and the
 * master key's key as the encryption key (see keystore.[ch]).
 */

/** Master key for wallet encryption */
class CMasterKey {
public:
    std::vector<unsigned char> vchCryptedKey;
    std::vector<unsigned char> vchSalt;

    unsigned int nDerivationMethod;
    unsigned int nDeriveIterations;

    std::vector<unsigned char> vchOtherDerivationParameters;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(vchCryptedKey);
        READWRITE(vchSalt);
        READWRITE(nDerivationMethod);
        READWRITE(nDeriveIterations);
        READWRITE(vchOtherDerivationParameters);
    }

    CMasterKey()
    {

        nDeriveIterations = 25000;
        nDerivationMethod = 0;
        vchOtherDerivationParameters = std::vector<unsigned char>(0);
    }
};

typedef std::vector<unsigned char, secure_allocator<unsigned char> > CKeyingMaterial;

namespace wallet_crypto {
class TestCrypter;
}

bool EncryptSecret(const CKeyingMaterial& vMasterKey, const CKeyingMaterial& vchPlaintext, const std::vector<unsigned char>& nIV, std::vector<unsigned char>& vchCiphertext);
bool EncryptSecret(const CKeyingMaterial& vMasterKey, const CKeyingMaterial& vchPlaintext, const uint256& nIV, std::vector<unsigned char>& vchCiphertext);
bool DecryptSecret(const CKeyingMaterial& vMasterKey, const std::vector<unsigned char>& vchCiphertext, const std::vector<unsigned char>& nIV, CKeyingMaterial& vchPlaintext);
bool DecryptSecret(const CKeyingMaterial& vMasterKey, const std::vector<unsigned char>& vchCiphertext, const uint256& nIV, CKeyingMaterial& vchPlaintext);

/** Encryption/decryption context with key information */
class CCrypter {
    friend class wallet_crypto::TestCrypter; // for test access to chKey/chIV
private:
    unsigned char chKey[WALLET_CRYPTO_KEY_SIZE];
    unsigned char chIV[WALLET_CRYPTO_IV_SIZE];
    bool fKeySet;

    int BytesToKeySHA512AES(const std::vector<unsigned char>& chSalt, const SecureString& strKeyData, int count, unsigned char* key, unsigned char* iv) const;

public:
    bool SetKeyFromPassphrase(const SecureString& strKeyData, const std::vector<unsigned char>& chSalt, const unsigned int nRounds, const unsigned int nDerivationMethod);
    bool Encrypt(const CKeyingMaterial& vchPlaintext, std::vector<unsigned char>& vchCiphertext) const;
    bool Decrypt(const std::vector<unsigned char>& vchCiphertext, CKeyingMaterial& vchPlaintext) const;
    bool SetKey(const CKeyingMaterial& chNewKey, const std::vector<unsigned char>& chNewIV);

    void CleanKey()
    {
        memory_cleanse(chKey, sizeof(chKey));
        memory_cleanse(chIV, sizeof(chIV));
        fKeySet = false;
    }

    CCrypter()
    {
        fKeySet = false;

        LockedPageManager::Instance().LockRange(&chKey[0], sizeof chKey);
        LockedPageManager::Instance().LockRange(&chIV[0], sizeof chIV);
    }

    ~CCrypter()
    {
        CleanKey();

        LockedPageManager::Instance().UnlockRange(&chKey[0], sizeof chKey);
        LockedPageManager::Instance().UnlockRange(&chIV[0], sizeof chIV);
    }
};

/** Keystore which keeps the private keys encrypted.
 * It derives from the basic key store, which is used if no encryption is active.
 */
class CCryptoKeyStore : public CBasicKeyStore {
private:
    CryptedKeyMap mapCryptedKeys;

    CKeyingMaterial vMasterKey;

    bool fUseCrypto;

    bool fDecryptionThoroughlyChecked;

protected:
    virtual bool SetCrypted();

    virtual bool EncryptKeys(CKeyingMaterial& vMasterKeyIn);

    virtual bool Unlock(const CKeyingMaterial& vMasterKeyIn);

    CCryptoKeyStore()
        : CBasicKeyStore()
        , fUseCrypto(false)
        , fDecryptionThoroughlyChecked(false)
    {
    }

    virtual bool IsCrypted() const
    {
        return fUseCrypto;
    }

    virtual bool IsLocked() const
    {
        if (!IsCrypted())
            return false;
        bool result;
        {
            LOCK(cs_KeyStore);
            result = vMasterKey.empty();
        }
        return result;
    }

    virtual bool Lock();

    virtual bool AddCryptedKey(const CPubKey& vchPubKey, const std::vector<unsigned char>& vchCryptedSecret);
    virtual bool AddKeyPubKey(const CKey& key, const CPubKey& pubkey);
    virtual bool AddKeyPubKey(int64_t HDKeyIndex, const CPubKey& pubkey);
    virtual bool HaveKey(const CKeyID& address) const
    {
        {
            LOCK(cs_KeyStore);
            if (!IsCrypted())
                return CBasicKeyStore::HaveKey(address);
            return mapCryptedKeys.count(address) > 0;
        }
        return false;
    }
    virtual bool GetKey(const CKeyID& address, std::vector<unsigned char>& encryptedKeyOut) const;
    virtual bool GetKey(const CKeyID& address, CKey& keyOut) const;
    virtual bool GetKey(const CKeyID& address, int64_t& HDKeyIndex) const;
    virtual bool GetPubKey(const CKeyID& address, CPubKey& vchPubKeyOut) const;
    virtual void GetKeys(std::set<CKeyID>& setAddress) const
    {
        if (!IsCrypted()) {
            CBasicKeyStore::GetKeys(setAddress);
            return;
        }
        setAddress.clear();
        CryptedKeyMap::const_iterator mi = mapCryptedKeys.begin();
        while (mi != mapCryptedKeys.end()) {
            setAddress.insert((*mi).first);
            mi++;
        }
    }

public:
    /**
     * Wallet status (encrypted, locked) changed.
     * Note: Called without locks held.
     */
    boost::signals2::signal<void(CCryptoKeyStore* wallet)> NotifyStatusChanged;
    friend class CAccount;
    friend class CAccountHD;
    friend class CWallet;
};

#endif // BITCOIN_WALLET_CRYPTER_H
