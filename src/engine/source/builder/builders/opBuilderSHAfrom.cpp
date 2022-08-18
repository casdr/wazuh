#include "opBuilderSHAfrom.hpp"

#include <optional>
#include <string>

#include <fmt/format.h>
#include <json/json.hpp>

#include <openssl/evp.h>
#include <openssl/sha.h>

#include "baseTypes.hpp"
#include <baseHelper.hpp>

namespace
{

// Sha1 digest len (20) * 2 (hex chars per byte)
constexpr int OS_SHA1_HEXDIGEST_SIZE = (SHA_DIGEST_LENGTH * 2);
constexpr int OS_SHA1_ARRAY_SIZE_LEN = OS_SHA1_HEXDIGEST_SIZE + 1;

std::optional<std::string> wdbStringsHash(const std::vector<std::string>& input)
{
    char* parameter = NULL;
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_size;

    EVP_MD_CTX* ctx = EVP_MD_CTX_create();
    if (!ctx)
    {
        // Failed during hash context creation
        return std::nullopt;
    }

    if (1 != EVP_DigestInit_ex(ctx, EVP_sha1(), NULL))
    {
        // Failed during hash context initialization
        EVP_MD_CTX_destroy(ctx);
        return std::nullopt;
    }

    for (const auto& word : input)
    {
        if (1 != EVP_DigestUpdate(ctx, word.c_str(), word.length()))
        {
            // Failed during hash context update
            return std::nullopt;
        }
    }

    EVP_DigestFinal_ex(ctx, digest, &digest_size);
    EVP_MD_CTX_destroy(ctx);

    // OS_SHA1_Hexdigest(digest, hexdigest);
    char output[OS_SHA1_ARRAY_SIZE_LEN];
    for (size_t n = 0; n < SHA_DIGEST_LENGTH; n++)
    {
        sprintf(&output[n * 2], "%02x", digest[n]);
    }

    return {output};
}

} // namespace

namespace builder::internals::builders
{

// field: +sha1_from/<string1>|<string_reference1>/<string2>|<string_reference2>
base::Expression opBuilderSHAfrom(const std::any& definition)
{
    auto [targetField, name, rawParameters] = helper::base::extractDefinition(definition);
    auto parameters = helper::base::processParameters(rawParameters);

    // Assert expected minimun number of parameters
    helper::base::checkParametersMinSize(parameters, 1);
    // Format name for the tracer
    name = helper::base::formatHelperFilterName(name, targetField, parameters);

    // Tracing
    const auto successTrace = fmt::format("[{}] -> Success", name);
    const auto failureTrace1 =
        fmt::format("[{}] -> Failure: Argument shouldn't be empty", name);
    const auto failureTrace2 = fmt::format(
        "[{}] -> Failure: Couldn't create HASH and write it in the JSON", name);

    // Return Term
    return base::Term<base::EngineOp>::create(
        name,
        [=, targetField = std::move(targetField), parameters = std::move(parameters)](
            base::Event event) -> base::result::Result<base::Event>
        {
            std::vector<std::string> resolvedParameter;
            // Check parameters
            for (auto& param : parameters)
            {
                // Getting array field name
                if (param.m_type == helper::base::Parameter::Type::REFERENCE)
                {
                    const auto s_paramValue {event->getString(param.m_value)};
                    if (!s_paramValue.has_value())
                    {
                        return base::result::makeFailure(event, failureTrace1);
                    }
                    resolvedParameter.emplace_back(s_paramValue.value());
                }
                else
                {
                    resolvedParameter.emplace_back(param.m_value);
                }
            }

            auto resultHash = wdbStringsHash(resolvedParameter);
            if (!resultHash.has_value())
            {
                return base::result::makeFailure(event, failureTrace2);
            }
            event->setString(resultHash.value(), targetField);
            return base::result::makeSuccess(event, successTrace);
        });
}

} // namespace builder::internals::builders
