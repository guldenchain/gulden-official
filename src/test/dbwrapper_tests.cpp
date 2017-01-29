// Copyright (c) 2012-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "dbwrapper.h"
#include "uint256.h"
#include "random.h"
#include "test/test_bitcoin.h"

#include <boost/assign/std/vector.hpp> // for 'operator+=()'
#include <boost/assert.hpp>
#include <boost/test/unit_test.hpp>

using namespace std;
using namespace boost::assign; // bring 'operator+=()' into scope
using namespace boost::filesystem;

// Test if a string consists entirely of null characters
bool is_null_key(const vector<unsigned char>& key)
{
    bool isnull = true;

    for (unsigned int i = 0; i < key.size(); i++)
        isnull &= (key[i] == '\x00');

    return isnull;
}

BOOST_FIXTURE_TEST_SUITE(dbwrapper_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(dbwrapper)
{

    for (int i = 0; i < 2; i++) {
        bool obfuscate = (bool)i;
        path ph = temp_directory_path() / unique_path();
        CDBWrapper dbw(ph, (1 << 20), true, false, obfuscate);
        char key = 'k';
        uint256 in = GetRandHash();
        uint256 res;

        BOOST_CHECK(obfuscate != is_null_key(dbwrapper_private::GetObfuscateKey(dbw)));

        BOOST_CHECK(dbw.Write(key, in));
        BOOST_CHECK(dbw.Read(key, res));
        BOOST_CHECK_EQUAL(res.ToString(), in.ToString());
    }
}

BOOST_AUTO_TEST_CASE(dbwrapper_compression)
{
    // Perform tests both with compression and without
    for (int i = 0; i < 2; i++) {
        bool compression = (bool)i;
        path ph = temp_directory_path() / unique_path();
        CDBWrapper dbw(ph, (1 << 20), true, false, false, compression);
        char key = 'k';
        uint256 in = GetRandHash();
        uint256 res;

        BOOST_CHECK(dbw.Write(key, in));
        BOOST_CHECK(dbw.Read(key, res));
        BOOST_CHECK_EQUAL(res.ToString(), in.ToString());
    }
}

BOOST_AUTO_TEST_CASE(dbwrapper_maxopenfiles_64)
{
    path ph = temp_directory_path() / unique_path();
    CDBWrapper dbw(ph, (1 << 20), true, false, false, false, 64);
    char key = 'k';
    uint256 in = GetRandHash();
    uint256 res;

    BOOST_CHECK(dbw.Write(key, in));
    BOOST_CHECK(dbw.Read(key, res));
    BOOST_CHECK_EQUAL(res.ToString(), in.ToString());
}

BOOST_AUTO_TEST_CASE(dbwrapper_maxopenfiles_1000)
{
    path ph = temp_directory_path() / unique_path();
    CDBWrapper dbw(ph, (1 << 20), true, false, false, false, 1000);
    char key = 'k';
    uint256 in = GetRandHash();
    uint256 res;

    BOOST_CHECK(dbw.Write(key, in));
    BOOST_CHECK(dbw.Read(key, res));
    BOOST_CHECK_EQUAL(res.ToString(), in.ToString());
}

BOOST_AUTO_TEST_CASE(dbwrapper_batch)
{

    for (int i = 0; i < 2; i++) {
        bool obfuscate = (bool)i;
        path ph = temp_directory_path() / unique_path();
        CDBWrapper dbw(ph, (1 << 20), true, false, obfuscate);

        char key = 'i';
        uint256 in = GetRandHash();
        char key2 = 'j';
        uint256 in2 = GetRandHash();
        char key3 = 'k';
        uint256 in3 = GetRandHash();

        uint256 res;
        CDBBatch batch(dbw);

        batch.Write(key, in);
        batch.Write(key2, in2);
        batch.Write(key3, in3);

        batch.Erase(key3);

        dbw.WriteBatch(batch);

        BOOST_CHECK(dbw.Read(key, res));
        BOOST_CHECK_EQUAL(res.ToString(), in.ToString());
        BOOST_CHECK(dbw.Read(key2, res));
        BOOST_CHECK_EQUAL(res.ToString(), in2.ToString());

        BOOST_CHECK(dbw.Read(key3, res) == false);
    }
}

BOOST_AUTO_TEST_CASE(dbwrapper_iterator)
{

    for (int i = 0; i < 2; i++) {
        bool obfuscate = (bool)i;
        path ph = temp_directory_path() / unique_path();
        CDBWrapper dbw(ph, (1 << 20), true, false, obfuscate);

        char key = 'j';
        uint256 in = GetRandHash();
        BOOST_CHECK(dbw.Write(key, in));
        char key2 = 'k';
        uint256 in2 = GetRandHash();
        BOOST_CHECK(dbw.Write(key2, in2));

        boost::scoped_ptr<CDBIterator> it(const_cast<CDBWrapper*>(&dbw)->NewIterator());

        it->Seek(key);

        char key_res;
        uint256 val_res;

        it->GetKey(key_res);
        it->GetValue(val_res);
        BOOST_CHECK_EQUAL(key_res, key);
        BOOST_CHECK_EQUAL(val_res.ToString(), in.ToString());

        it->Next();

        it->GetKey(key_res);
        it->GetValue(val_res);
        BOOST_CHECK_EQUAL(key_res, key2);
        BOOST_CHECK_EQUAL(val_res.ToString(), in2.ToString());

        it->Next();
        BOOST_CHECK_EQUAL(it->Valid(), false);
    }
}

BOOST_AUTO_TEST_CASE(existing_data_no_obfuscate)
{

    path ph = temp_directory_path() / unique_path();
    create_directories(ph);

    CDBWrapper* dbw = new CDBWrapper(ph, (1 << 10), false, false, false);
    char key = 'k';
    uint256 in = GetRandHash();
    uint256 res;

    BOOST_CHECK(dbw->Write(key, in));
    BOOST_CHECK(dbw->Read(key, res));
    BOOST_CHECK_EQUAL(res.ToString(), in.ToString());

    delete dbw;

    CDBWrapper odbw(ph, (1 << 10), false, false, true);

    uint256 res2;
    BOOST_CHECK(odbw.Read(key, res2));
    BOOST_CHECK_EQUAL(res2.ToString(), in.ToString());

    BOOST_CHECK(!odbw.IsEmpty()); // There should be existing data
    BOOST_CHECK(is_null_key(dbwrapper_private::GetObfuscateKey(odbw))); // The key should be an empty string

    uint256 in2 = GetRandHash();
    uint256 res3;

    BOOST_CHECK(odbw.Write(key, in2));
    BOOST_CHECK(odbw.Read(key, res3));
    BOOST_CHECK_EQUAL(res3.ToString(), in2.ToString());
}

BOOST_AUTO_TEST_CASE(existing_data_reindex)
{

    path ph = temp_directory_path() / unique_path();
    create_directories(ph);

    CDBWrapper* dbw = new CDBWrapper(ph, (1 << 10), false, false, false);
    char key = 'k';
    uint256 in = GetRandHash();
    uint256 res;

    BOOST_CHECK(dbw->Write(key, in));
    BOOST_CHECK(dbw->Read(key, res));
    BOOST_CHECK_EQUAL(res.ToString(), in.ToString());

    delete dbw;

    CDBWrapper odbw(ph, (1 << 10), false, true, true);

    uint256 res2;
    BOOST_CHECK(!odbw.Read(key, res2));
    BOOST_CHECK(!is_null_key(dbwrapper_private::GetObfuscateKey(odbw)));

    uint256 in2 = GetRandHash();
    uint256 res3;

    BOOST_CHECK(odbw.Write(key, in2));
    BOOST_CHECK(odbw.Read(key, res3));
    BOOST_CHECK_EQUAL(res3.ToString(), in2.ToString());
}

BOOST_AUTO_TEST_CASE(iterator_ordering)
{
    path ph = temp_directory_path() / unique_path();
    CDBWrapper dbw(ph, (1 << 20), true, false, false);
    for (int x = 0x00; x < 256; ++x) {
        uint8_t key = x;
        uint32_t value = x * x;
        BOOST_CHECK(dbw.Write(key, value));
    }

    boost::scoped_ptr<CDBIterator> it(const_cast<CDBWrapper*>(&dbw)->NewIterator());
    for (int c = 0; c < 2; ++c) {
        int seek_start;
        if (c == 0)
            seek_start = 0x00;
        else
            seek_start = 0x80;
        it->Seek((uint8_t)seek_start);
        for (int x = seek_start; x < 256; ++x) {
            uint8_t key;
            uint32_t value;
            BOOST_CHECK(it->Valid());
            if (!it->Valid()) // Avoid spurious errors about invalid iterator's key and value in case of failure
                break;
            BOOST_CHECK(it->GetKey(key));
            BOOST_CHECK(it->GetValue(value));
            BOOST_CHECK_EQUAL(key, x);
            BOOST_CHECK_EQUAL(value, x * x);
            it->Next();
        }
        BOOST_CHECK(!it->Valid());
    }
}

struct StringContentsSerializer {

    string str;
    StringContentsSerializer() {}
    StringContentsSerializer(const string& inp)
        : str(inp)
    {
    }

    StringContentsSerializer& operator+=(const string& s)
    {
        str += s;
        return *this;
    }
    StringContentsSerializer& operator+=(const StringContentsSerializer& s) { return *this += s.str; }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        if (ser_action.ForRead()) {
            str.clear();
            char c = 0;
            while (true) {
                try {
                    READWRITE(c);
                    str.push_back(c);
                }
                catch (const std::ios_base::failure& e) {
                    break;
                }
            }
        } else {
            for (size_t i = 0; i < str.size(); i++)
                READWRITE(str[i]);
        }
    }
};

BOOST_AUTO_TEST_CASE(iterator_string_ordering)
{
    char buf[10];

    path ph = temp_directory_path() / unique_path();
    CDBWrapper dbw(ph, (1 << 20), true, false, false);
    for (int x = 0x00; x < 10; ++x) {
        for (int y = 0; y < 10; y++) {
            sprintf(buf, "%d", x);
            StringContentsSerializer key(buf);
            for (int z = 0; z < y; z++)
                key += key;
            uint32_t value = x * x;
            BOOST_CHECK(dbw.Write(key, value));
        }
    }

    boost::scoped_ptr<CDBIterator> it(const_cast<CDBWrapper*>(&dbw)->NewIterator());
    for (int c = 0; c < 2; ++c) {
        int seek_start;
        if (c == 0)
            seek_start = 0;
        else
            seek_start = 5;
        sprintf(buf, "%d", seek_start);
        StringContentsSerializer seek_key(buf);
        it->Seek(seek_key);
        for (int x = seek_start; x < 10; ++x) {
            for (int y = 0; y < 10; y++) {
                sprintf(buf, "%d", x);
                string exp_key(buf);
                for (int z = 0; z < y; z++)
                    exp_key += exp_key;
                StringContentsSerializer key;
                uint32_t value;
                BOOST_CHECK(it->Valid());
                if (!it->Valid()) // Avoid spurious errors about invalid iterator's key and value in case of failure
                    break;
                BOOST_CHECK(it->GetKey(key));
                BOOST_CHECK(it->GetValue(value));
                BOOST_CHECK_EQUAL(key.str, exp_key);
                BOOST_CHECK_EQUAL(value, x * x);
                it->Next();
            }
        }
        BOOST_CHECK(!it->Valid());
    }
}

BOOST_AUTO_TEST_SUITE_END()
