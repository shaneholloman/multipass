/*
 * Copyright (C) Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "common.h"
#include "file_operations.h"
#include "mock_file_ops.h"
#include "mock_logger.h"
#include "mock_openssl_syscalls.h"
#include "mock_ssh.h"
#include "mock_ssh_process_exit_status.h"
#include "mock_ssh_test_fixture.h"
#include "mock_virtual_machine.h"
#include "stub_ssh_key_provider.h"
#include "temp_dir.h"
#include "temp_file.h"

#include <multipass/format.h>
#include <multipass/utils.h>
#include <multipass/vm_image_vault.h>

#include <QRegularExpression>

#include <gtest/gtest-death-test.h>

#include <sstream>
#include <string>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpt = multipass::test;
namespace mpu = mp::utils;

using namespace testing;

namespace
{
std::string file_contents{"line 1 of file contents\nline 2\n"};

void check_file_contents(QFile& checked_file, const std::string& checked_contents)
{
    checked_file.open(QIODevice::ReadOnly | QIODevice::Text);

    QString actual_contents;

    while (!checked_file.atEnd())
        actual_contents += checked_file.readLine();

    checked_file.close();

    ASSERT_EQ(checked_contents, actual_contents.toStdString());
}
} // namespace

TEST(Utils, hostnameBeginsWithLetterIsValid)
{
    EXPECT_TRUE(mp::utils::valid_hostname("foo"));
}

TEST(Utils, hostnameSingleLetterIsValid)
{
    EXPECT_TRUE(mp::utils::valid_hostname("f"));
}

TEST(Utils, hostnameContainsDigitIsValid)
{
    EXPECT_TRUE(mp::utils::valid_hostname("foo1"));
}

TEST(Utils, hostnameContainsHyphenIsValid)
{
    EXPECT_TRUE(mp::utils::valid_hostname("foo-bar"));
}

TEST(Utils, hostnameBeginsWithDigitIsInvalid)
{
    EXPECT_FALSE(mp::utils::valid_hostname("1foo"));
}

TEST(Utils, hostnameSingleDigitIsInvalid)
{
    EXPECT_FALSE(mp::utils::valid_hostname("1"));
}

TEST(Utils, hostnameContainsUnderscoreIsInvalid)
{
    EXPECT_FALSE(mp::utils::valid_hostname("foo_bar"));
}

TEST(Utils, hostnameContainsSpecialCharacterIsInvalid)
{
    EXPECT_FALSE(mp::utils::valid_hostname("foo!"));
}

TEST(Utils, pathRootInvalid)
{
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("//")));
}

TEST(Utils, pathRootFooValid)
{
    EXPECT_FALSE(mp::utils::invalid_target_path(QString("/foo")));
    EXPECT_FALSE(mp::utils::invalid_target_path(QString("/foo/")));
    EXPECT_FALSE(mp::utils::invalid_target_path(QString("//foo")));
}

TEST(Utils, pathDevInvalid)
{
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/dev")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/dev/")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("//dev/")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/dev//")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("//dev//")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/dev/foo")));
}

TEST(Utils, pathDevpathValid)
{
    EXPECT_FALSE(mp::utils::invalid_target_path(QString("/devpath")));
    EXPECT_FALSE(mp::utils::invalid_target_path(QString("/devpath/")));
    EXPECT_FALSE(mp::utils::invalid_target_path(QString("/devpath/foo")));
}

TEST(Utils, pathProcInvalid)
{
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/proc")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/proc/")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("//proc/")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/proc//")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("//proc//")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/proc/foo")));
}

TEST(Utils, pathSysInvalid)
{
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/sys")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/sys/")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("//sys/")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/sys//")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("//sys//")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/sys/foo")));
}

TEST(Utils, pathHomeProperInvalid)
{
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/home")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/home/")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("//home/")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/home//")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("//home//")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/home/foo/..")));
}

TEST(Utils, pathHomeUbuntuInvalid)
{
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/home/ubuntu")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/home/ubuntu/")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("//home/ubuntu/")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/home//ubuntu/")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/home/ubuntu//")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("//home//ubuntu//")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/home/ubuntu/foo/..")));
}

TEST(Utils, pathHomeFooValid)
{
    EXPECT_FALSE(mp::utils::invalid_target_path(QString("/home/foo")));
    EXPECT_FALSE(mp::utils::invalid_target_path(QString("/home/foo/")));
    EXPECT_FALSE(mp::utils::invalid_target_path(QString("//home/foo/")));
}

TEST(Utils, pathHomeUbuntuFooValid)
{
    EXPECT_FALSE(mp::utils::invalid_target_path(QString("/home/ubuntu/foo")));
    EXPECT_FALSE(mp::utils::invalid_target_path(QString("/home/ubuntu/foo/")));
    EXPECT_FALSE(mp::utils::invalid_target_path(QString("//home/ubuntu/foo")));
}

TEST(Utils, makeFileWithContentWorks)
{
    mpt::TempDir temp_dir;
    QString file_name = temp_dir.path() + "/test-file";

    EXPECT_NO_THROW(MP_UTILS.make_file_with_content(file_name.toStdString(), file_contents));

    QFile checked_file(file_name);
    check_file_contents(checked_file, file_contents);
}

TEST(Utils, makeFileWithContentDoesNotOverwrite)
{
    mpt::TempDir temp_dir;
    QString file_name = temp_dir.path() + "/test-file";

    EXPECT_NO_THROW(MP_UTILS.make_file_with_content(file_name.toStdString(), file_contents));

    QFile checked_file(file_name);
    check_file_contents(checked_file, file_contents);

    MP_EXPECT_THROW_THAT(MP_UTILS.make_file_with_content(file_name.toStdString(), "other stuff\n"),
                         std::runtime_error,
                         mpt::match_what(HasSubstr("already exists")));

    check_file_contents(checked_file, file_contents);
}

TEST(Utils, makeFileWithContentOverwritesWhenAsked)
{
    mpt::TempDir temp_dir;
    QString file_name = temp_dir.path() + "/test-file";

    EXPECT_NO_THROW(MP_UTILS.make_file_with_content(file_name.toStdString(), file_contents));

    QFile checked_file(file_name);
    check_file_contents(checked_file, file_contents);

    EXPECT_NO_THROW(
        MP_UTILS.make_file_with_content(file_name.toStdString(), "other stuff\n", true));

    check_file_contents(checked_file, "other stuff\n");
}

TEST(Utils, makeFileWithContentCreatesPath)
{
    mpt::TempDir temp_dir;
    QString file_name = temp_dir.path() + "/new_dir/test-file";

    EXPECT_NO_THROW(MP_UTILS.make_file_with_content(file_name.toStdString(), file_contents));

    QFile checked_file(file_name);
    check_file_contents(checked_file, file_contents);
}

TEST(Utils, makeFileWithContentFailsIfPathCannotBeCreated)
{
    std::string file_name{"some_dir/test-file"};

    auto [mock_file_ops, guard] = mpt::MockFileOps::inject();

    EXPECT_CALL(*mock_file_ops, exists(A<const QFile&>())).WillOnce(Return(false));
    EXPECT_CALL(*mock_file_ops, mkpath(_, _)).WillOnce(Return(false));

    MP_EXPECT_THROW_THAT(MP_UTILS.make_file_with_content(file_name, file_contents),
                         std::runtime_error,
                         mpt::match_what(HasSubstr("failed to create dir")));
}

TEST(Utils, makeFileWithContentFailsIfFileCannotBeCreated)
{
    std::string file_name{"some_dir/test-file"};

    auto [mock_file_ops, guard] = mpt::MockFileOps::inject();

    EXPECT_CALL(*mock_file_ops, exists(A<const QFile&>())).WillOnce(Return(false));
    EXPECT_CALL(*mock_file_ops, mkpath(_, _)).WillOnce(Return(true));
    EXPECT_CALL(*mock_file_ops, open(_, _)).WillOnce(Return(false));

    MP_EXPECT_THROW_THAT(MP_UTILS.make_file_with_content(file_name, file_contents),
                         std::runtime_error,
                         mpt::match_what(HasSubstr("failed to open file")));
}

TEST(Utils, makeFileWithContentThrowsOnWriteError)
{
    std::string file_name{"some_dir/test-file"};

    auto [mock_file_ops, guard] = mpt::MockFileOps::inject();

    EXPECT_CALL(*mock_file_ops, exists(A<const QFile&>())).WillOnce(Return(false));
    EXPECT_CALL(*mock_file_ops, mkpath(_, _)).WillOnce(Return(true));
    EXPECT_CALL(*mock_file_ops, open(_, _)).WillOnce(Return(true));
    EXPECT_CALL(*mock_file_ops, write(A<QFile&>(), _, _)).WillOnce(Return(747));

    MP_EXPECT_THROW_THAT(MP_UTILS.make_file_with_content(file_name, file_contents),
                         std::runtime_error,
                         mpt::match_what(HasSubstr("failed to write to file")));
}

TEST(Utils, makeFileWithContentThrowsOnFailureToFlush)
{
    std::string file_name{"some_dir/test-file"};

    auto [mock_file_ops, guard] = mpt::MockFileOps::inject();

    EXPECT_CALL(*mock_file_ops, exists(A<const QFile&>())).WillOnce(Return(false));
    EXPECT_CALL(*mock_file_ops, mkpath(_, _)).WillOnce(Return(true));
    EXPECT_CALL(*mock_file_ops, open(_, _)).WillOnce(Return(true));
    EXPECT_CALL(*mock_file_ops, write(A<QFile&>(), _, _)).WillOnce(Return(file_contents.size()));
    EXPECT_CALL(*mock_file_ops, flush(A<QFile&>())).WillOnce(Return(false));

    MP_EXPECT_THROW_THAT(MP_UTILS.make_file_with_content(file_name, file_contents),
                         std::runtime_error,
                         mpt::match_what(HasSubstr("failed to flush file")));
}

TEST(Utils, expectedScryptHashReturned)
{
    const auto passphrase = MP_UTILS.generate_scrypt_hash_for("passphrase");

    EXPECT_EQ(passphrase,
              "f28cb995d91eed8064674766f28e468aae8065b2cf02af556c857dd77de2d2476f3830fd02147f3e3503"
              "7a1812df"
              "0d0d0934fa677be585269fee5358d5c70758");
}

TEST(Utils, generateScryptHashErrorThrows)
{
    REPLACE(EVP_PBE_scrypt, [](auto...) { return 0; });

    MP_EXPECT_THROW_THAT(MP_UTILS.generate_scrypt_hash_for("passphrase"),
                         std::runtime_error,
                         mpt::match_what(StrEq("Cannot generate passphrase hash")));
}

TEST(Utils, toCmdReturnsEmptyStringOnEmptyInput)
{
    std::vector<std::string> args{};
    auto output = mp::utils::to_cmd(args, mp::utils::QuoteType::quote_every_arg);
    EXPECT_THAT(output, ::testing::StrEq(""));
}

TEST(Utils, toCmdOutputAreNotEscapedWithNoQuotes)
{
    std::vector<std::string> args{"hello", "world"};
    auto output = mp::utils::to_cmd(args, mp::utils::QuoteType::no_quotes);
    EXPECT_THAT(output, ::testing::StrEq("hello world"));
}

TEST(Utils, toCmdArgumentsAreNotEscapedIfNotNeeded)
{
    std::vector<std::string> args{"hello", "world"};
    auto output = mp::utils::to_cmd(args, mp::utils::QuoteType::quote_every_arg);
    EXPECT_THAT(output, ::testing::StrEq("hello world"));
}

TEST(Utils, toCmdArgumentsWithSingleQuotesAreEscaped)
{
    std::vector<std::string> args{"it's", "me"};
    auto output = mp::utils::to_cmd(args, mp::utils::QuoteType::quote_every_arg);
    EXPECT_THAT(output, ::testing::StrEq("it\\'s me"));
}

TEST(Utils, toCmdArgumentsWithDoubleQuotesAreEscaped)
{
    std::vector<std::string> args{"they", "said", "\"please\""};
    auto output = mp::utils::to_cmd(args, mp::utils::QuoteType::quote_every_arg);
    EXPECT_THAT(output, ::testing::StrEq("they said \\\"please\\\""));
}

struct TestTrimUtilities : public Test
{
    std::string s{"\n \f \n \r \t   \vI'm a great\n\t string \n \f \n \r \t   \v"};
};

TEST_F(TestTrimUtilities, trimEndActuallyTrimsEnd)
{
    mp::utils::trim_end(s);

    EXPECT_THAT(s, ::testing::StrEq("\n \f \n \r \t   \vI'm a great\n\t string"));
}

TEST_F(TestTrimUtilities, trimBeginActuallyTrimsTheBeginning)
{
    mp::utils::trim_begin(s);

    EXPECT_EQ(s, "I'm a great\n\t string \n \f \n \r \t   \v");
}

TEST_F(TestTrimUtilities, trimActuallyTrims)
{
    mp::utils::trim(s);

    EXPECT_EQ(s, "I'm a great\n\t string");
}

TEST_F(TestTrimUtilities, trimAcceptsCustomFilter)
{
    mp::utils::trim(s, [](unsigned char c) { return c == '\n' || c == '\v'; });

    EXPECT_EQ(s, " \f \n \r \t   \vI'm a great\n\t string \n \f \n \r \t   ");
}

TEST(Utils, trimNewlineWorks)
{
    std::string s{"correct\n"};
    mp::utils::trim_newline(s);

    EXPECT_THAT(s, ::testing::StrEq("correct"));
}

TEST(Utils, trimNewlineAssertionWorks)
{
    std::string s{"wrong"};
    // https://google.github.io/googletest/advanced.html#regular-expression-syntax
    ASSERT_DEBUG_DEATH(mp::utils::trim_newline(s), "\\wssert");
}

TEST(Utils, escapeForShellActuallyEscapes)
{
    std::string s{"I've got \"quotes\""};
    auto res = mp::utils::escape_for_shell(s);
    EXPECT_THAT(res, ::testing::StrEq("I\\'ve\\ got\\ \\\"quotes\\\""));
}

TEST(Utils, escapeForShellQuotesNewlines)
{
    std::string s{"I've got\nnewlines"};
    auto res = mp::utils::escape_for_shell(s);
    EXPECT_THAT(res, ::testing::StrEq("I\\'ve\\ got\"\n\"newlines"));
}

TEST(Utils, escapeForShellQuotesEmptyString)
{
    std::string s{""};
    auto res = mp::utils::escape_for_shell(s);
    EXPECT_THAT(res, ::testing::StrEq("''"));
}

TEST(Utils, tryActionActuallyTimesOut)
{
    bool on_timeout_called{false};
    auto on_timeout = [&on_timeout_called] { on_timeout_called = true; };
    auto retry_action = [] { return mp::utils::TimeoutAction::retry; };
    mp::utils::try_action_for(on_timeout, std::chrono::milliseconds(1), retry_action);

    EXPECT_TRUE(on_timeout_called);
}

TEST(Utils, tryActionDoesNotTimeout)
{
    bool on_timeout_called{false};
    auto on_timeout = [&on_timeout_called] { on_timeout_called = true; };

    bool action_called{false};
    auto successful_action = [&action_called] {
        action_called = true;
        return mp::utils::TimeoutAction::done;
    };
    mp::utils::try_action_for(on_timeout, std::chrono::seconds(1), successful_action);

    EXPECT_FALSE(on_timeout_called);
    EXPECT_TRUE(action_called);
}

TEST(Utils, uuidHasNoCurlyBrackets)
{
    auto uuid = mp::utils::make_uuid();
    EXPECT_FALSE(uuid.contains(QRegularExpression("[{}]")));
}

TEST(Utils, contentsOfActuallyReadsContents)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";
    std::string expected_content{"just a bit of test content here"};
    mpt::make_file_with_content(file_name, expected_content);

    auto content = mp::utils::contents_of(file_name);
    EXPECT_THAT(content, StrEq(expected_content));
}

TEST(Utils, contentsOfThrowsOnMissingFile)
{
    EXPECT_THROW(mp::utils::contents_of("this-file-does-not-exist"), std::runtime_error);
}

TEST(Utils, contentsOfEmptyContentsOnEmptyFile)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/empty_test_file";
    mpt::make_file_with_content(file_name, "");

    auto content = mp::utils::contents_of(file_name);
    EXPECT_TRUE(content.empty());
}

TEST(Utils, splitReturnsTokenList)
{
    std::vector<std::string> expected_tokens;
    expected_tokens.push_back("Hello");
    expected_tokens.push_back("World");
    expected_tokens.push_back("Bye!");

    const std::string delimiter{":"};

    std::stringstream content;
    for (const auto& token : expected_tokens)
    {
        content << token;
        content << delimiter;
    }

    const auto tokens = mp::utils::split(content.str(), delimiter);
    EXPECT_THAT(tokens, ContainerEq(expected_tokens));
}

TEST(Utils, splitReturnsOneTokenIfNoDelimiter)
{
    const std::string content{"no delimiter here"};
    const std::string delimiter{":"};

    const auto tokens = mp::utils::split(content, delimiter);
    ASSERT_THAT(tokens.size(), Eq(1u));

    EXPECT_THAT(tokens[0], StrEq(content));
}

TEST(Utils, validMacAddressWorks)
{
    EXPECT_TRUE(mp::utils::valid_mac_address("00:11:22:33:44:55"));
    EXPECT_TRUE(mp::utils::valid_mac_address("aa:bb:cc:dd:ee:ff"));
    EXPECT_TRUE(mp::utils::valid_mac_address("AA:BB:CC:DD:EE:FF"));
    EXPECT_TRUE(mp::utils::valid_mac_address("52:54:00:dd:ee:ff"));
    EXPECT_TRUE(mp::utils::valid_mac_address("52:54:00:AB:CD:EF"));
    EXPECT_FALSE(mp::utils::valid_mac_address("01:23:45:67:89:AG"));
    EXPECT_FALSE(mp::utils::valid_mac_address("012345678901"));
    EXPECT_FALSE(mp::utils::valid_mac_address("1:23:45:65:89:ab"));
    EXPECT_FALSE(mp::utils::valid_mac_address("aa-bb-cc-dd-ee-ff"));
    EXPECT_FALSE(mp::utils::valid_mac_address("aa:bb:cc:dd:ee:ff:"));
    EXPECT_FALSE(mp::utils::valid_mac_address(":aa:bb:cc:dd:ee:ff"));
}

TEST(Utils, hasOnlyDigitsWorks)
{
    EXPECT_FALSE(mp::utils::has_only_digits("124ft:,"));
    EXPECT_TRUE(mp::utils::has_only_digits("0123456789"));
    EXPECT_FALSE(mp::utils::has_only_digits("0123456789:'`'"));
}

TEST(Utils, randomBytesReturnCorrectSize)
{
    EXPECT_THAT(MP_UTILS.random_bytes(4), SizeIs(4));
}

TEST(Utils, validateServerAddressThrowsOnInvalidAddress)
{
    EXPECT_THROW(mp::utils::validate_server_address("unix"), std::runtime_error);
    EXPECT_THROW(mp::utils::validate_server_address("unix:"), std::runtime_error);
    EXPECT_THROW(mp::utils::validate_server_address("test:test"), std::runtime_error);
    EXPECT_THROW(mp::utils::validate_server_address(""), std::runtime_error);
}

TEST(Utils, validateServerAddressDoesNotThrowOnGoodAddress)
{
    EXPECT_NO_THROW(mp::utils::validate_server_address("unix:/tmp/a_socket"));
    EXPECT_NO_THROW(mp::utils::validate_server_address("test-server.net:123"));
}

TEST(Utils, noSubdirectoryReturnsSamePath)
{
    mp::Path original_path{"/tmp/foo"};
    QString empty_subdir{};

    EXPECT_THAT(mp::utils::backend_directory_path(original_path, empty_subdir), Eq(original_path));
}

TEST(Utils, subdirectoryReturnsNewPath)
{
    mp::Path original_path{"/tmp/foo"};
    QString subdir{"bar"};

    EXPECT_THAT(mp::utils::backend_directory_path(original_path, subdir),
                Eq(mp::Path{"/tmp/foo/bar"}));
}

TEST(Utils, vmRunningReturnsTrue)
{
    mp::VirtualMachine::State state = mp::VirtualMachine::State::running;

    EXPECT_TRUE(MP_UTILS.is_running(state));
}

TEST(Utils, vmDelayedShutdownReturnsTrue)
{
    mp::VirtualMachine::State state = mp::VirtualMachine::State::delayed_shutdown;

    EXPECT_TRUE(MP_UTILS.is_running(state));
}

TEST(Utils, vmStoppedReturnsFalse)
{
    mp::VirtualMachine::State state = mp::VirtualMachine::State::stopped;

    EXPECT_FALSE(MP_UTILS.is_running(state));
}

TEST(Utils, absentConfigFileAndDirAreCreated)
{
    mpt::TempDir temp_dir;
    const QString config_file_path{QString("%1/config_dir/config").arg(temp_dir.path())};

    mp::utils::check_and_create_config_file(config_file_path);

    EXPECT_TRUE(QFile::exists(config_file_path));
}

TEST(Utils, existingConfigFileIsUntouched)
{
    mpt::TempFile config_file;
    QFileInfo config_file_info{config_file.name()};

    auto original_last_modified = config_file_info.lastModified();

    mp::utils::check_and_create_config_file(config_file.name());

    auto new_last_modified = config_file_info.lastModified();

    EXPECT_THAT(new_last_modified, Eq(original_last_modified));
}

TEST(Utils, lineMatcherReturnsExpectedLine)
{
    std::string data{"LD_LIBRARY_PATH=/foo/lib\nSNAP=/foo/bin\nDATA=/bar/baz\n"};
    std::string matcher{"SNAP="};

    auto snap_data = mp::utils::match_line_for(data, matcher);

    EXPECT_THAT(snap_data, Eq("SNAP=/foo/bin"));
}

TEST(Utils, lineMatcherNoMatchReturnsEmptyString)
{
    std::string data{"LD_LIBRARY_PATH=/foo/lib\nSNAP=/foo/bin\nDATA=/bar/baz\n"};
    std::string matcher{"FOO="};

    auto snap_data = mp::utils::match_line_for(data, matcher);

    EXPECT_TRUE(snap_data.empty());
}

TEST(Utils, makeDirCreatesCorrectDir)
{
    mpt::TempDir temp_dir;
    QString new_dir{"foo"};

    auto new_path = MP_UTILS.make_dir(QDir(temp_dir.path()), new_dir);

    EXPECT_TRUE(QFile::exists(new_path));
    EXPECT_EQ(new_path, temp_dir.path() + "/" + new_dir);
}

TEST(Utils, makeDirWithNoNewDir)
{
    mpt::TempDir temp_dir;

    auto new_path = MP_UTILS.make_dir(QDir(temp_dir.path()), "");

    EXPECT_TRUE(QFile::exists(new_path));
    EXPECT_EQ(new_path, temp_dir.path());
}

TEST(Utils, checkFilesystemBytesAvailableReturnsNonNegative)
{
    mpt::TempDir temp_dir;

    auto bytes_available = MP_UTILS.filesystem_bytes_available(temp_dir.path());

    EXPECT_GE(bytes_available, 0);
}
